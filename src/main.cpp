#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Adafruit_MCP23X17.h>

#define DEVICE_NAME "Streamdeck"

#define INTPin 13
#define ledPin 2

const int encoder1PinA = 15;
const int encoder1PinB = 14;
const int encoder1PinSwitch = 33;

const int encoder2PinA = 26;
const int encoder2PinB = 17;
const int encoder2PinSwitch = 32;

int encoder1Pos = 0;
int encoder1PinALast = LOW;
int encoder1PinANow = LOW;

int encoder2Pos = 0;
int encoder2PinALast = LOW;
int encoder2PinANow = LOW;

bool encoder1Switch = LOW;
bool encoder1SwitchLast = LOW;
bool encoder2Switch = LOW;
bool encoder2SwitchLast = LOW;

#define numSwitches 15

bool switchStates[numSwitches];

bool deviceConnected = false;

int interval = 50;
unsigned long lastMillisLED = 0;

const int ledFreqDisconnected = 2000;
const int ledFreqConnected = 100;
const int ledDim = 50;

bool ledState;

int lastState[numSwitches];

int buttonMap[numSwitches] = {10, 13, 0, 1, 4, 9, 12, 15, 2, 5, 8, 11, 14, 3, 6};

const int encoder1Id = -1;
const int encoder2Id = -2;

int getButtonIdFormPin(int buttonId)
{
  for (int i = 0; i <= numSwitches; ++i)
  {
    if (buttonMap[i] == buttonId)
    {
      return i; // return i+1;
    }
  }
}

Adafruit_MCP23X17 mcp;

#define SERVICE_UUID "34b4daf9-ff80-4e58-a497-40d349f78692"

#define KEY_PRESS_CHARACTERISTIC_UUID "2d15f60d-56c6-47e4-8af6-e9d67077c09c"
#define ENCODER1_CHARACTERISTIC_UUID "101a1ee2-5b79-4b2f-b53d-cee26dff68d8"
#define ENCODER2_CHARACTERISTIC_UUID "b7c0ca66-4dac-4c06-82ba-05d38cec1fa4"
#define CONFIG_CHARACTERISTIC_UUID "006f82e3-fafa-44d9-82ac-add23151a870"

static NimBLEServer *pServer;
static NimBLEService *pService;
static NimBLECharacteristic *keyPressCharacteristic;
static NimBLECharacteristic *encoder1Characteristic;
static NimBLECharacteristic *encoder2Characteristic;
static NimBLECharacteristic *configCharacteristic;
static NimBLEAdvertising *pAdvertising;

void writeCharacteristic(NimBLECharacteristic *characteristic, String value)
{
  characteristic->setValue(value);
  characteristic->notify(true);
}

class ServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc)
  {
    deviceConnected = true;
    Serial.print("Client address: ");
    Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    NimBLEDevice::stopAdvertising();
    // pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
  };
  void onDisconnect(NimBLEServer *pServer, ble_gap_conn_desc *desc)
  {
    deviceConnected = false;
    Serial.print("Client disconnected: ");
    Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    NimBLEDevice::startAdvertising();
  };
};
static ServerCallbacks serverCB;

class CharacteristicsCallback : public NimBLECharacteristicCallbacks
{
  void onRead(NimBLECharacteristic *pCharacteristic)
  {

    Serial.print(pCharacteristic->getUUID().toString().c_str());
    Serial.print(": onRead(), value: ");
    Serial.println(pCharacteristic->getValue().c_str());
  };

  void onWrite(NimBLECharacteristic *pCharacteristic)
  {
    String pUUID = pCharacteristic->getUUID().toString().c_str();
    String pValue = pCharacteristic->getValue().c_str();
    Serial.print(pCharacteristic->getUUID().toString().c_str());
    Serial.print(": onWrite(), value: ");
    Serial.println(pCharacteristic->getValue().c_str());
    if (pUUID == CONFIG_CHARACTERISTIC_UUID)
    {
      Serial.print("new config: ");
      Serial.println(pValue);
    }
  };
};
static CharacteristicsCallback characteristicsCallback;

class DescriptorCallbacks : public NimBLEDescriptorCallbacks
{
  void onWrite(NimBLEDescriptor *pDescriptor)
  {
    std::string dscVal = pDescriptor->getValue();
    Serial.print("Descriptor witten value:");
    Serial.println(dscVal.c_str());
  };

  void onRead(NimBLEDescriptor *pDescriptor)
  {
    Serial.print(pDescriptor->getUUID().toString().c_str());
    Serial.println(" Descriptor read");
  };
};
static DescriptorCallbacks dscCallbacks;

void statLED(bool state)
{
  if (state)
  {
    ledcWrite(0, ledDim);
  }
  else
  {
    ledcWrite(0, 0);
  }
}

void setup()
{
  Serial.begin(115200);

  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCB);
  pService = pServer->createService(SERVICE_UUID);
  keyPressCharacteristic = pService->createCharacteristic(KEY_PRESS_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  configCharacteristic = pService->createCharacteristic(CONFIG_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::WRITE);
  encoder1Characteristic = pService->createCharacteristic(ENCODER1_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  encoder2Characteristic = pService->createCharacteristic(ENCODER2_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  keyPressCharacteristic->setCallbacks(&characteristicsCallback);
  encoder1Characteristic->setCallbacks(&characteristicsCallback);
  encoder2Characteristic->setCallbacks(&characteristicsCallback);
  configCharacteristic->setCallbacks(&characteristicsCallback);
  pService->start();
  pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setScanResponse(false);
  pAdvertising->start();

  pinMode(encoder1PinA, INPUT_PULLUP);
  pinMode(encoder1PinB, INPUT_PULLUP);
  pinMode(encoder1PinSwitch, INPUT_PULLUP);
  pinMode(encoder2PinA, INPUT_PULLUP);
  pinMode(encoder2PinB, INPUT_PULLUP);
  pinMode(encoder2PinSwitch, INPUT_PULLUP);
  pinMode(INTPin, INPUT);
  pinMode(ledPin, OUTPUT);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(ledPin, 0);
  delay(5000);

  if (!mcp.begin_I2C())
  {
    Serial.println("error while connecting to io expander");
    while (1)
      ;
  }
  Serial.println("connected to expander");

  mcp.setupInterrupts(true, false, LOW);

  for (int ID = 0; ID < 16; ID++)
  {
    mcp.pinMode(ID, INPUT_PULLUP);
    mcp.setupInterruptPin(ID, CHANGE);
  }

  memset(switchStates, false, sizeof(switchStates));
}

void loop()
{
  if (deviceConnected)
  {
    if (digitalRead(INTPin) == LOW)
    {
      int ID = getButtonIdFormPin(mcp.getLastInterruptPin());
//      bool state = mcp.digitalRead(ID);

      bool state = !lastState[ID];

      lastState[ID] = state;

      Serial.print("Switch changed: " + String(ID) + " ");
      Serial.println("New state: " + String(state));
      switchStates[ID] = !state;
      writeCharacteristic(keyPressCharacteristic, String(String(ID) + ";" + String(state)));
      mcp.clearInterrupts();
    }
    encoder1Switch = !digitalRead(encoder1PinSwitch);
    encoder2Switch = !digitalRead(encoder2PinSwitch);
    if (encoder1Switch != encoder1SwitchLast)
    {
      Serial.print("Encoder changed: " + String(-encoder1Id) + " ");
      Serial.println("New state: " + String(encoder1Switch));
      writeCharacteristic(keyPressCharacteristic, String(String(encoder1Id) + ";" + String(encoder1Switch)));
      encoder1SwitchLast = encoder1Switch;
    }
    if (encoder2Switch != encoder2SwitchLast)
    {
      Serial.print("Encoder changed: " + String(-encoder2Id) + " ");
      Serial.println("New state: " + String(encoder2Switch));
      writeCharacteristic(keyPressCharacteristic, String(String(encoder2Id) + ";" + String(encoder2Switch)));
      encoder2SwitchLast = encoder2Switch;
    }

    encoder1PinANow = digitalRead(encoder1PinA);
    if ((encoder1PinALast == HIGH) && (encoder1PinANow == LOW))
    {
      if (digitalRead(encoder1PinB) == HIGH)
      {
        encoder1Pos++;
      }
      else
      {
        encoder1Pos--;
      }
      Serial.println(encoder1Pos);
      writeCharacteristic(encoder1Characteristic, String(encoder1Pos));
    }
    encoder1PinALast = encoder1PinANow;
    encoder2PinANow = digitalRead(encoder2PinA);
    if ((encoder2PinALast == HIGH) && (encoder2PinANow == LOW))
    {
      if (digitalRead(encoder2PinB) == HIGH)
      {
        encoder2Pos++;
      }
      else
      {
        encoder2Pos--;
      }
      Serial.println(encoder2Pos);
      writeCharacteristic(encoder2Characteristic, String(encoder2Pos));
    }
    encoder2PinALast = encoder2PinANow;

    if ((millis() - lastMillisLED) > ledFreqConnected)
    {
      ledState = !ledState;
      statLED(ledState);
      lastMillisLED = millis();
    }
  }
  else
  {
    if ((millis() - lastMillisLED) > ledFreqDisconnected)
    {
      ledState = !ledState;
      statLED(ledState);
      lastMillisLED = millis();
    }
    mcp.clearInterrupts();
  }
}
