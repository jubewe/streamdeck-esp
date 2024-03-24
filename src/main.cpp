#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Adafruit_MCP23X17.h>
#include <TFT_eSPI.h>
#include <Adafruit_MAX1704X.h>
#include <BleKeyboard.h>
#include <Preferences.h>

#define DEVICE_NAME "Streamdeck"
#define DEVICE_NAME_CONFIG "Streamdeck Configuration"
#define DEVICE_FIRMWARE "V1.3"

std::map<std::string, uint8_t> customKeyMap = {
    {"KEY_LEFT_CTRL", KEY_LEFT_CTRL},
    {"KEY_LEFT_SHIFT", KEY_LEFT_SHIFT},
    {"KEY_LEFT_ALT", KEY_LEFT_ALT},
    {"KEY_LEFT_GUI", KEY_LEFT_GUI},
    {"KEY_RIGHT_CTRL", KEY_RIGHT_CTRL},
    {"KEY_RIGHT_SHIFT", KEY_RIGHT_SHIFT},
    {"KEY_RIGHT_ALT", KEY_RIGHT_ALT},
    {"KEY_RIGHT_GUI", KEY_RIGHT_GUI},
    {"KEY_UP_ARROW", KEY_UP_ARROW},
    {"KEY_DOWN_ARROW", KEY_DOWN_ARROW},
    {"KEY_LEFT_ARROW", KEY_LEFT_ARROW},
    {"KEY_RIGHT_ARROW", KEY_RIGHT_ARROW},
    {"KEY_BACKSPACE", KEY_BACKSPACE},
    {"KEY_TAB", KEY_TAB},
    {"KEY_RETURN", KEY_RETURN},
    {"KEY_ESC", KEY_ESC},
    {"KEY_INSERT", KEY_INSERT},
    {"KEY_PRTSC", KEY_PRTSC},
    {"KEY_DELETE", KEY_DELETE},
    {"KEY_PAGE_UP", KEY_PAGE_UP},
    {"KEY_PAGE_DOWN", KEY_PAGE_DOWN},
    {"KEY_HOME", KEY_HOME},
    {"KEY_END", KEY_END},
    {"KEY_CAPS_LOCK", KEY_CAPS_LOCK},
    {"KEY_F1", KEY_F1},
    {"KEY_F2", KEY_F2},
    {"KEY_F3", KEY_F3},
    {"KEY_F4", KEY_F4},
    {"KEY_F5", KEY_F5},
    {"KEY_F6", KEY_F6},
    {"KEY_F7", KEY_F7},
    {"KEY_F8", KEY_F8},
    {"KEY_F9", KEY_F9},
    {"KEY_F10", KEY_F10},
    {"KEY_F11", KEY_F11},
    {"KEY_F12", KEY_F12},
    {"KEY_F13", KEY_F13},
    {"KEY_F14", KEY_F14},
    {"KEY_F15", KEY_F15},
    {"KEY_F16", KEY_F16},
    {"KEY_F17", KEY_F17},
    {"KEY_F18", KEY_F18},
    {"KEY_F19", KEY_F19},
    {"KEY_F20", KEY_F20},
    {"KEY_F21", KEY_F21},
    {"KEY_F22", KEY_F22},
    {"KEY_F23", KEY_F23},
    {"KEY_F24", KEY_F24},
    {"KEY_NUM_0", KEY_NUM_0},
    {"KEY_NUM_1", KEY_NUM_1},
    {"KEY_NUM_2", KEY_NUM_2},
    {"KEY_NUM_3", KEY_NUM_3},
    {"KEY_NUM_4", KEY_NUM_4},
    {"KEY_NUM_5", KEY_NUM_5},
    {"KEY_NUM_6", KEY_NUM_6},
    {"KEY_NUM_7", KEY_NUM_7},
    {"KEY_NUM_8", KEY_NUM_8},
    {"KEY_NUM_9", KEY_NUM_9},
    {"KEY_NUM_SLASH", KEY_NUM_SLASH},
    {"KEY_NUM_ASTERISK", KEY_NUM_ASTERISK},
    {"KEY_NUM_MINUS", KEY_NUM_MINUS},
    {"KEY_NUM_PLUS", KEY_NUM_PLUS},
    {"KEY_NUM_ENTER", KEY_NUM_ENTER},
    {"KEY_NUM_PERIOD", KEY_NUM_PERIOD}};

// settings
bool setting_saveOldMenuPage = false;

Adafruit_MAX17048 lipo;
BleKeyboard bleKeyboard;
Preferences preferences;

#define INTPin 13
#define ledPin 2

const int tftBLPin = 4;

int batteryPercent = 0;
int batteryPercentOld = -1;
bool refreshScreen = true;
bool config;

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

const int ledFreqDisconnected = 200;
const int ledFreqConnected = 2000;
const int ledDim = 50;

bool ledState;

int lastState[numSwitches];

int buttonMap[numSwitches] = {10, 13, 0, 1, 4, 9, 12, 15, 2, 5, 8, 11, 14, 3, 6};

const int encoder1Id = -1;
const int encoder2Id = -2;

int getButtonIdFromPin(int buttonId)
{
  for (int i = 0; i <= numSwitches; i++)
  {
    if (buttonMap[i] == buttonId)
    {
      return i; // return i+1;
    }
  }
}

Adafruit_MCP23X17 mcp;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite img = TFT_eSprite(&tft);

#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 80

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
  std::string stdString = std::string(value.c_str());

  characteristic->setValue(stdString);
  characteristic->notify(true);
}

class ServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc)
  {
    Serial.println("updateMTU to: " + String(NimBLEDevice::getMTU()));
    // Serial.println("updateMTU to: " + String(pServer->getPeerMTU(desc->conn_handle)));
    deviceConnected = true;
    Serial.print("Client address: ");
    // NimBLEDevice::stopAdvertising();
    NimBLEDevice::startAdvertising();

    // pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
    refreshScreen = true;
  };
  void onDisconnect(NimBLEServer *pServer, ble_gap_conn_desc *desc)
  {
    deviceConnected = false;
    Serial.print("Client disconnected: ");
    NimBLEDevice::startAdvertising();
    refreshScreen = true;
  };
};
static ServerCallbacks serverCB;

class CharacteristicsCallback : public NimBLECharacteristicCallbacks
{
  void onRead(NimBLECharacteristic *pCharacteristic)
  {
    String pUUID = pCharacteristic->getUUID().toString().c_str();
    String pValue = pCharacteristic->getValue().c_str();
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

      // pattern: g,"id","info/value",{{"config"}},"clipboard"
      if (pValue[0] == 'c')
      {
        int seperator1 = pValue.indexOf(",");
        int seperator2 = pValue.indexOf(",", seperator1 + 1);
        int seperator3 = pValue.indexOf(",{{", seperator2 + 1);
        int seperator4 = pValue.indexOf("}},", seperator3 + 1);
        String id = pValue.substring(seperator1 + 1, seperator2);
        String value = pValue.substring(seperator2 + 1, seperator3);
        String config = pValue.substring(seperator3 + 3, seperator4);
        bool clipboard = pValue.substring(seperator4 + 3, pValue.length()) == "1";
        preferences.putString(id.c_str(), value);
        preferences.putString((String(id) + "config").c_str(), config);
        preferences.putBool((id + "clipboard").c_str(), clipboard);

        Serial.print("new config: ");
        Serial.println(pValue);
        Serial.println(id);
        Serial.println(value);
        Serial.println(config);
        Serial.println(clipboard);
      }
      // pattern: g,"id"
      if (pValue[0] == 'g')
      {
        int seperator1 = pValue.indexOf(",");
        String id = pValue.substring(seperator1 + 1, pValue.length());
        String value = preferences.getString(id.c_str(), "---");
        String config = preferences.getString((String(id) + "config").c_str(), "");
        bool clipboard = preferences.getBool((id + "clipboard").c_str(), false);
        Serial.print("send config: ");
        Serial.println(pValue);
        Serial.println(id);
        Serial.println(value);
        Serial.println(config);
        Serial.println(clipboard);
        writeCharacteristic(configCharacteristic, "s," + id + "," + value + ",{{" + config + "}}," + String(clipboard));
      }
    }
  };
};
static CharacteristicsCallback characteristicsCallback;

class DescriptorCallbacks : public NimBLEDescriptorCallbacks
{
  void onWrite(NimBLEDescriptor *pDescriptor){
      // std::string dscVal = pDescriptor->getValue();
      // Serial.print("Descriptor witten value:");
      // Serial.println(dscVal.c_str());
  };

  void onRead(NimBLEDescriptor *pDescriptor){
      // Serial.print(pDescriptor->getUUID().toString().c_str());
      // Serial.println(" Descriptor read");
  };
};
static DescriptorCallbacks dscCallbacks;

/**/
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

void drawBackground()
{
  tft.fillRoundRect(2, 2, DISPLAY_WIDTH - 4, DISPLAY_HEIGHT - 4, 2, TFT_BLACK);
  tft.drawCentreString(DEVICE_NAME, (DISPLAY_WIDTH - 45 - 10) / 2, 10, 2);
  tft.drawCentreString(DEVICE_FIRMWARE, (DISPLAY_WIDTH - 45 - 10) / 2, 33 - 5, 1);

  if (deviceConnected)
  {
    tft.drawCentreString("  connected  ", (DISPLAY_WIDTH - 45 - 10) / 2, 65, 1);
    tft.drawRoundRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 10, TFT_SKYBLUE);
  }
  else
  {
    tft.drawCentreString("disconnected", (DISPLAY_WIDTH - 45 - 10) / 2, 65, 1);
    tft.drawRoundRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 10, TFT_DARKGREY);
  }
  if (config)
  {
    tft.drawCentreString("(configuration)", (DISPLAY_WIDTH - 45 - 10) / 2, 50, 1);
    tft.drawRoundRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 10, TFT_GREEN);
  }
  else
  {
    tft.drawCentreString("               ", (DISPLAY_WIDTH - 45 - 10) / 2, 50, 1);
  }
}

int page = 0;
int pageMax = 2;
int pageOld = 0;

void drawPage()
{
  tft.drawCentreString(String(page + 1), DISPLAY_WIDTH - 47 - 3, DISPLAY_HEIGHT / 2 - 10, 4);
  tft.drawCircle(DISPLAY_WIDTH - 48 - 3, DISPLAY_HEIGHT / 2, 14, TFT_LIGHTGREY);
}

void drawKeyString(char key, int id)
{
  tft.fillRoundRect(2, 2, DISPLAY_WIDTH - 4, DISPLAY_HEIGHT - 4, 4, TFT_BLACK);
  String value = preferences.getString(String(id).c_str(), "---");
  tft.drawCentreString("key " + String(id) + " / page " + String(page + 1) + ":", (DISPLAY_WIDTH) / 2, DISPLAY_HEIGHT / 2 - 30, 2);

  tft.drawCentreString(value, (DISPLAY_WIDTH) / 2, DISPLAY_HEIGHT / 2 - 10, 2);
}

void drawBattery(int batteryPercent)
{
  int color = TFT_GREEN;
  if (batteryPercent <= 30)
  {
    color = TFT_RED;
  }
  else if (batteryPercent <= 45)
  {
    color = TFT_ORANGE;
  }
  img.setColorDepth(8);
  img.createSprite(38, DISPLAY_HEIGHT - 3);
  img.fillSprite(TFT_TRANSPARENT);
  img.drawRoundRect(40 - 20 - 8, 17, 20, 45, 3, TFT_WHITE);
  img.fillRect(40 - 15 - 8, 14, 10, 3, TFT_WHITE);
  img.fillRoundRect(40 - 15 - 8, 12, 10, 5, 2, TFT_WHITE);
  img.fillRoundRect(40 - 19 - 8, 17 + 1, 18, constrain(45 - (map(batteryPercent, 0, 100, 5, 45)), 3, 45), 2, TFT_BLACK);
  img.fillRoundRect(40 - 19 - 8, constrain(17 + 45 - (map(batteryPercent, 0, 100, 5, 45)) + 1, 18, 68), 18, constrain((map(batteryPercent, 0, 100, 5, 45)) - 2, 0, 48), 2, color);
  img.fillRect(40 - 25 - 8, 63, 30, 30, TFT_BLACK);
  img.drawCentreString(String(constrain(batteryPercent, 0, 99)) + "%", 40 - 10 - 8, 63, 2);
  img.pushSprite(DISPLAY_WIDTH - 41, -3, TFT_TRANSPARENT);
  img.deleteSprite();
}

void setup()
{
  Serial.begin(115200);
  pinMode(encoder1PinA, INPUT_PULLUP);
  pinMode(encoder1PinB, INPUT_PULLUP);
  pinMode(encoder1PinSwitch, INPUT_PULLUP);
  pinMode(encoder2PinA, INPUT_PULLUP);
  pinMode(encoder2PinB, INPUT_PULLUP);
  pinMode(encoder2PinSwitch, INPUT_PULLUP);
  pinMode(tftBLPin, OUTPUT);
  pinMode(INTPin, INPUT);
  pinMode(ledPin, OUTPUT);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(ledPin, 0);
  config = !digitalRead(encoder2PinSwitch) || !digitalRead(encoder1PinSwitch);
  if (config)
  {
    // NimBLEDevice::setMTU(BLE_ATT_MTU_MAX);
    NimBLEDevice::init(DEVICE_NAME_CONFIG);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(BLE_ATT_MTU_MAX);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCB);
    pService = pServer->createService(SERVICE_UUID);
    keyPressCharacteristic = pService->createCharacteristic(KEY_PRESS_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    configCharacteristic = pService->createCharacteristic(CONFIG_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE | NIMBLE_PROPERTY::WRITE);
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
  }
  else
  {
    bleKeyboard.setName(DEVICE_NAME);
    bleKeyboard.begin();
  }

  preferences.begin("prefs");

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  digitalWrite(tftBLPin, HIGH);

  if (!mcp.begin_I2C())
  {
    Serial.println("error while connecting to io expander");
    while (1)
      ;
  }
  Serial.println("connected to expander");
  if (!lipo.begin())
  {
    Serial.println("error while connecting to MAX17048");
  }
  else
  {
    Serial.println("connected to MAX17048");
  }
  mcp.setupInterrupts(true, false, LOW);

  for (int ID = 0; ID < 16; ID++)
  {
    mcp.pinMode(ID, INPUT_PULLUP);
    mcp.setupInterruptPin(ID, CHANGE);
  }

  memset(switchStates, false, sizeof(switchStates));
}

int i = 0;
unsigned long lastMillisBatteryRead;
int batteryReadFreq = 1000;

#define menusNum 3
#define menusSwitchThreshold 4
bool showKeyString = false;
bool showKeyStringOld = false;
bool setKeyMode = false;
bool menuMode = false;
bool inSubmenu = false;
bool showKeyDisabled = false;
bool pageSwitchDisabled = false;
int menuPage = 0;
int menuEncoderPos = 0;
int menuEncoderPosLast = 0;

// last menu page = exit menu

bool closeMenu = false;

void loop()
{
  if (menuMode)
  {
    showKeyString = false;
  }
  else
  {
    showKeyString = !digitalRead(encoder2PinSwitch);
  }

  if (showKeyString && !showKeyStringOld)
  {
    Serial.println("showKeyString press");
    showKeyStringOld = showKeyString;
  }
  if (!showKeyString && showKeyStringOld)
  {
    Serial.println("showKeyString release");
    showKeyStringOld = showKeyString;
    refreshScreen = true;
  }

  if (page != pageOld)
  {
    drawPage();
    pageOld = page;
  }

  if (!config)
  {
    bool keyboardConnected = bleKeyboard.isConnected();
    if (deviceConnected != keyboardConnected)
    {
      deviceConnected = keyboardConnected;
      refreshScreen = true;
    }
  }
  if (batteryPercent != batteryPercentOld)
  {
    bleKeyboard.setBatteryLevel(batteryPercent);
    drawBattery(batteryPercent);
    batteryPercentOld = batteryPercent;
  }
  if (refreshScreen)
  {
    drawBackground();
    drawBattery(batteryPercent);
    drawPage();
    refreshScreen = false;
  }
  if ((millis() - lastMillisBatteryRead) > batteryReadFreq)
  {
    batteryPercent = constrain(int(lipo.cellPercent()), 0, 99);
    // Serial.println("battery level: " + String(batteryPercent) + "%");
    // Serial.println("battery voltage: " + String(lipo.cellVoltage()) + "V");
    Serial.println("updateMTU0 to: " + String(NimBLEDevice::getMTU()));
    bleKeyboard.setBatteryLevel(batteryPercent);
    lastMillisBatteryRead = millis();
  }

  encoder1Switch = !digitalRead(encoder1PinSwitch);
  encoder2Switch = !digitalRead(encoder2PinSwitch);

  if ((encoder1Switch == HIGH && encoder2Switch == HIGH) && (encoder1SwitchLast == LOW || encoder2SwitchLast == LOW))
  {
    if (inSubmenu)
    {
      inSubmenu = false;
    }
    else
    {
      menuMode = !menuMode;
      inSubmenu = false;
      if (!setting_saveOldMenuPage)
      {
        menuPage = 0;
        menuEncoderPosLast = 0;
      }
      showKeyString = false;
      Serial.print("Menu ");
      Serial.println(menuMode ? "show" : "hide");
    }
  }
  else if (menuMode && !inSubmenu && encoder1SwitchLast == LOW && encoder2SwitchLast == LOW && encoder2Switch == HIGH)
  {
    inSubmenu = true;

    switch (menuPage)
    {
    case menusNum - 1:
    {
      closeMenu = true;
    }
    }
  }

  if (closeMenu)
  {
    menuMode = false;
    inSubmenu = false;
    if (!setting_saveOldMenuPage)
    {
      menuPage = 0;
      menuEncoderPosLast = 0;
    }

    closeMenu = false;
  }

  if (encoder1Switch != encoder1SwitchLast)
  {
    Serial.print("Encoder changed: " + String(-encoder1Id) + " ");
    Serial.println("New state: " + String(encoder1Switch));
    // writeCharacteristic(keyPressCharacteristic, String(String(encoder1Id) + ";" + String(encoder1Switch)));
    encoder1SwitchLast = encoder1Switch;
  }
  if (encoder2Switch != encoder2SwitchLast)
  {
    Serial.print("Encoder changed: " + String(-encoder2Id) + " ");
    Serial.println("New state: " + String(encoder2Switch));
    // writeCharacteristic(keyPressCharacteristic, String(String(encoder2Id) + ";" + String(encoder2Switch)));
    encoder2SwitchLast = encoder2Switch;
  }

  encoder1PinANow = digitalRead(encoder1PinA);
  if ((encoder1PinALast == HIGH) && (encoder1PinANow == LOW))
  {
    if (digitalRead(encoder1PinB) == HIGH)
    {
      if (encoder1Switch)
      {
        if (page < pageMax)
        {
          page++;
        }
        else
        {
          page = 0;
        }
      }
      else
      {
        encoder1Pos++;
      }
    }
    else
    {
      if (encoder1Switch)
      {
        if (page > 0)
        {
          page--;
        }
        else
        {
          page = pageMax;
        }
      }
      else
      {
        encoder1Pos--;
      }
    }
    Serial.println(encoder1Pos);
    // writeCharacteristic(encoder1Characteristic, String(encoder1Pos));
  }

  if ((encoder1PinANow == HIGH))
  {
  }
  encoder1PinALast = encoder1PinANow;

  encoder2PinANow = digitalRead(encoder2PinA);
  if ((encoder2PinALast == HIGH) && (encoder2PinANow == LOW))
  {
    if (digitalRead(encoder2PinB) == HIGH)
    {
      if (menuMode)
      {
        if (!inSubmenu)
        {
          if (menuEncoderPos < 0)
            menuEncoderPos = 0;
          menuEncoderPos++;
          if ((menuPage < menusNum - 1) && menuEncoderPos >= menusSwitchThreshold)
          {
            menuPage++;
            menuEncoderPos = 0;
          }
          menuEncoderPosLast++;
        }
      }
      else
        encoder2Pos++;
    }
    else
    {
      if (menuMode)
      {
        if (!inSubmenu)
        {
          if (menuEncoderPos > 0)
            menuEncoderPos = 0;
          menuEncoderPos--;
          if (menuPage > 0 && menuEncoderPos <= -menusSwitchThreshold)
          {
            menuPage--;
            menuEncoderPos = 0;
          }
        }
      }
      else
        encoder2Pos--;
    }
    Serial.println(encoder2Pos);
    Serial.println("menu page " + String(menuPage));
    // writeCharacteristic(encoder2Characteristic, String(encoder2Pos));
  }
  encoder2PinALast = encoder2PinANow;

  if (menuMode)
  {
    if (inSubmenu)
    {
      switch (menuPage)
      {
      case menusNum - 1:
      {
        // display exit

        break;
      }
      }
    }
  }

  if (digitalRead(INTPin) == LOW)
  {
    int lastInterrupted = mcp.getLastInterruptPin();
    int ID = getButtonIdFromPin(mcp.getLastInterruptPin());
    //      bool state = mcp.digitalRead(ID);

    bool state = !lastState[ID];

    lastState[ID] = state;
    char string = 'a' + ID + 15 * page;
    Serial.print("Switch changed: " + String(ID) + " ");
    Serial.println("New state: " + String(state));
    Serial.println("char:" + String(string));

    if (!menuMode && showKeyString && showKeyStringOld)
    {
      drawKeyString(string, ID);
    }
    // else if (((encoder1Switch == HIGH) || (encoder1SwitchLast == HIGH)) && (state == LOW))
    // {
    // }
    else
    {
      if (((encoder1Switch == HIGH) || (encoder1SwitchLast == HIGH)) && (state == HIGH))
      {
        // if (menuMode)
        // {
        //   if (ID <= menusNum)
        //   {
        //     menuPage = ID;
        //   }
        // }
        // else
        // {
        if (!pageSwitchDisabled)
        {
          Serial.println("Switch to page " + String(ID));
          if (ID <= pageMax)
          {
            page = ID;
          }
        }
        // }
      }
      else
      {
        if (!menuMode && !showKeyString)
        {
          bool clipboard = preferences.getBool((String(ID) + "clipboard").c_str(), false);
          String config = preferences.getString((String(ID) + "config").c_str(), "");
          if (state == HIGH && clipboard)
          {
            bleKeyboard.print(config);
          }
          else
          {
            int startPos = 0;     // Starting position for searching
            int separatorPos = 0; // Position of the '+' separator
            while (separatorPos >= 0)
            {
              separatorPos = config.indexOf('+', startPos); // Find the next '+' separator
              String part;
              if (separatorPos >= 0)
              {                                                  // If separator found
                part = config.substring(startPos, separatorPos); // Extract the part between separators
                startPos = separatorPos + 1;                     // Update starting position for the next search
              }
              else
              {                                    // If no more separators found
                part = config.substring(startPos); // Extract the last part
              }
              if (state)
              {
                u_int8_t key = customKeyMap[part.c_str()];
                if (key != 0)
                {
                  bleKeyboard.press(key);
                }
                else
                {
                  bleKeyboard.press(part[0]);
                }
                Serial.println("hold: " + part + "(" + String(key) + ")");
              }
            }
            if (!state)
            {
              bleKeyboard.releaseAll();
              Serial.println("release all");
            }
            /*
            if (state == HIGH)
            {
              bleKeyboard.press(KEY_LEFT_CTRL);
              bleKeyboard.press(KEY_LEFT_ALT);
              bleKeyboard.press(KEY_LEFT_SHIFT);
              //bleKeyboard.press(keyMap["KEY_LEFT_CTRL"]);
              //keyMap.find("KEY_LEFT_CTRL");
              bleKeyboard.press(string);
              delay(1);
              bleKeyboard.releaseAll();
              Serial.println("press");
            }*/
          }
          Serial.println("sent over ble");
        }
      }
    }

    switchStates[ID] = !state;
    // writeCharacteristic(keyPressCharacteristic, String(String(ID) + ";" + String(state)));
    mcp.clearInterrupts();
  }

  // Serial.println("Menu data: ");
  // Serial.print("enabled: " + String(menuMode));
  // Serial.print("in submenu: " + String(inSubmenu));
  // Serial.print("page: " + String(menuPage));

  if (deviceConnected)
  {
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
  }
}
