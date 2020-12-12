/*
 * Author: Dat Nguyen
 * Description: 
 * 1.0.0 Initial
 * 1.0.1 Remove software serial with hardware serial
 * 1.0.2 Remove heavy config saving and requesting process out of AsyncRequest context
 * 1.0.3 Reuse software serial + add SPIFFS
 * 1.0.4 Fix bug device_config deepCopy return null bug ;(
 * 1.1.0 When there are nodes connected to this AP --> decrease the config refresh rate to boost user experience
 * 1.1.1 Auto scaling config refresh rate when there are no connected devices and there are.
 * 1.2.0 Add LED light strip manament in this board
 * 1.3.0 Add toggle switches to turn on/off wifi and LED strip power
 * 1.3.1 Add more light mode
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h> //https://arduino-esp8266.readthedocs.io/en/latest/libraries.html#eeprom
#include <Adafruit_NeoPixel.h>

const char* SOFTWARE_VERSION = "1.3.1";

#define SOFT_SWITCH_POWER_MANAGEMENT_PIN 12 // ~D6
#define OWNERSHIP_WRITE_CHECK 123
#define DEVICE_CONFIG_EEPROM_PACKET_ADDRESS 0

#define STABLE_LIGHT_MODE 0
#define LIGHT_SPIN_LIGHT_MODE 1
#define NIGHT_MODE_LIGHT_MODE 2
#define RUNING_LIGHT_MODE 3

#define RGB_WIPE_LIGHT_MODE 100
#define RGB_FADE_IN_AND_OUT 106
#define RAINBOW_CYCLE_LIGHT_MODE 101
#define SMOOTH_RAINBOW_TRANSITION_LIGHT_MODE 102
#define THEATHER_CHASE_RAINBOW_LIGHT_MODE 103
#define LIGHT_SHOW_LIGHT_MODE01 104

void(* resetFunc) (void) = 0;

const char* ssid     = "LL Mini Infinity Mirror";
const char* password = "123456789";

AsyncWebServer server(80);

IPAddress local_IP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
  
typedef struct {
  byte ownership;
  byte intensity;
  byte _mode;
  byte red;
  byte green;
  byte blue;  
  int eepromWriteCycleCount;
} device_config __attribute__((packed));

device_config deviceConfig;
device_config toSavePayload;
device_config defaultDeviceConfig = {
  OWNERSHIP_WRITE_CHECK,
  50,
  SMOOTH_RAINBOW_TRANSITION_LIGHT_MODE,
  0,
  0,
  250,
  1
};


volatile boolean dirtySavePacket = false;
volatile boolean resetDeviceMode = false;

unsigned long lastSoftPowerReadTimer = millis();
boolean isDirtyCmdProcess() {
  if (millis() - lastSoftPowerReadTimer > 500) {
    lastSoftPowerReadTimer = millis();
    if (digitalRead(SOFT_SWITCH_POWER_MANAGEMENT_PIN) == LOW) { return true; }
  }
  return dirtySavePacket || resetDeviceMode;
}
String arduinoErrorRequestError = "No error";

/**************************LED STRIP*********************/
#define NEO_PIXEL_PIN 14 // ~D5
#define NUM_PIXEL 30
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXEL, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);

void stableLightMode(uint32_t color) {
  if (isDirtyCmdProcess()) { return; }
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

uint32_t getUserConfiguredStripColor() {
  return strip.Color(deviceConfig.red, deviceConfig.green, deviceConfig.blue);
}

//TODO: fix this function
boolean lightshow01() {
  strip.setBrightness(80);
  if (!colorWipe(strip.Color(255, 0, 0), 50)) { return false; }; // Red
  if (!colorWipe(strip.Color(0, 255, 0), 50)) { return false; }; // Green
  if (!colorWipe(strip.Color(0, 0, 255), 50)) { return false; }; // Blue
  if (!colorWipe(strip.Color(0, 0, 0, 255), 50)) { return false; }; // White RGBW
  // Send a theater pixel chase in...
  if (!theaterChase(strip.Color(127, 127, 127), 50)) { return false; }; // White
  if (!theaterChase(strip.Color(127, 0, 0), 50)) { return false; }; // Red
  if (!theaterChase(strip.Color(0, 0, 127), 50)) { return false; }; // Blue
  if (!rainbowCycle(10)) { return false; };
  if (!smoothRainbowCycle(10)) { return false; };
  if (!theaterChaseRainbow(30)) { return false; };
  if (!lightSpin(strip.Color(0, 255, 0), 5, 00, 40)) { return false; };
  if (!nightMode(strip.Color(0, 255, 0), 20, 50)) { return false; };
  return true;
}

boolean theaterChaseRainbow(uint8_t wait) {
  if (isDirtyCmdProcess()) { return false; }
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();
      if (isDirtyCmdProcess()) { return false; }
      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
      
    }
    if (isDirtyCmdProcess()) { return false; }
  }
  return true;
}

boolean colorWipe(uint32_t c, uint8_t wait) {
  if (isDirtyCmdProcess()) { return false; }
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
    if (isDirtyCmdProcess()) { return false; }
  }
  return true;
}

void showStrip() {
 strip.show();
}
void setPixel(int Pixel, byte red, byte green, byte blue) {
 strip.setPixelColor(Pixel, strip.Color(red, green, blue));
}

void setAll(byte red, byte green, byte blue) {
  for(int i = 0; i < NUM_PIXEL; i++ ) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}

boolean FadeInOut(byte red, byte green, byte blue, byte wait){
  if (isDirtyCmdProcess()) { return false; }
  float r, g, b;
     
  for(int k = 0; k < 256; k=k+1) {
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
    showStrip();
    delay(wait);
    if (isDirtyCmdProcess()) { return false; }
  }
     
  for(int k = 255; k >= 0; k=k-2) {
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
    showStrip();
    delay(wait);
    if (isDirtyCmdProcess()) { return false; }
  }
  return true;
}

boolean RunningLights(byte red, byte green, byte blue, int WaveDelay) {
  int Position=0;
  if (isDirtyCmdProcess()) { return false; }
  for(int j=0; j<NUM_PIXEL*5; j++)
  {
      Position++; // = 0; //Position + Rate;
      for(int i=0; i<NUM_PIXEL; i++) {
        // sine wave, 3 offset waves make a rainbow!
        //float level = sin(i+Position) * 127 + 128;
        //setPixel(i,level,0,0);
        //float level = sin(i+Position) * 127 + 128;
        setPixel(i,((sin(i+Position) * 127 + 128)/255)*red,
                   ((sin(i+Position) * 127 + 128)/255)*green,
                   ((sin(i+Position) * 127 + 128)/255)*blue);
      }
     
      showStrip();
      delay(WaveDelay);
      if (isDirtyCmdProcess()) { return false; }
  }
  return true;
}

boolean RGBfadeInAndOut() {
  if (!FadeInOut(255, 0, 0, 1)) { return false; } // red
  delay(300);
  if (!FadeInOut(0, 255, 0, 1)) { return false; } // white
  delay(300);
  if (!FadeInOut(0, 0, 255, 1)) { return false; } // blue
  delay(300);
  if (!FadeInOut(255,255,255, 1)) { return false; }
  delay(300);
  return true;
}



boolean lightSpin(uint32_t color, uint8_t group, uint16_t numSteps, uint8_t wait) {
  if (isDirtyCmdProcess()) { return false; }
  if (group >= strip.numPixels()) {
//    Serial.println("ERROR: lightSpin() group need to be less than numPixels");
    return false;
  }
  for (uint16_t i = 0; i < group; i++) {
    strip.setPixelColor(i, color);
    strip.show();
  }

  for (int i = group; i < numSteps; i++) {
    strip.setPixelColor(i % strip.numPixels(), color);
    strip.setPixelColor((i - group) % strip.numPixels(), strip.Color(0, 0, 0));
    strip.show();
    delay(wait);
    if (isDirtyCmdProcess()) { return false; }
  }
  return true;
}

boolean nightMode(uint32_t color, uint8_t wait, uint8_t top) {
  if (isDirtyCmdProcess()) { return false; }
  for (int i = 0; i <= top; i++) {
    strip.setBrightness(i);
    for (uint16_t j = 0; j < strip.numPixels(); j++) {
      strip.setPixelColor(j, color);
    }
    strip.show();
    if (i < 40) {
      delay(wait);
    }
    delay(wait);
    if (isDirtyCmdProcess()) { return false; }
  }
  delay(700);
  if (isDirtyCmdProcess()) { return false; }
  delay(700);
  if (isDirtyCmdProcess()) { return false; }
  for (int i = top; i >= 0; i--) {
    strip.setBrightness(i);
    for (uint16_t j = 0; j < strip.numPixels(); j++) {
      strip.setPixelColor(j, color);
    }
    strip.show();
    if (i < 40) {
      delay(wait);
    }
    delay(wait);
    if (isDirtyCmdProcess()) { return false; }
  }
  return true;
}

boolean smoothRainbowCycle(uint8_t wait) {
  if (isDirtyCmdProcess()) { return false ; }
  uint16_t i, j;
  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
    if (j%10 == 0 && isDirtyCmdProcess()) { return false; }
  }
  return true;
}

// Slightly different, this makes the rainbow equally distributed throughout
boolean rainbowCycle(uint8_t wait) {
  if (isDirtyCmdProcess()) { return false; }
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
    if (isDirtyCmdProcess()) { return false; }
  }
  return true;
}

//Theatre-style crawling lights.
boolean theaterChase(uint32_t c, uint8_t wait) {
  if (isDirtyCmdProcess()) { return false; }
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();
      if (isDirtyCmdProcess()) { return false; }
      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
    if (isDirtyCmdProcess()) { return false; }
  }
  return true;
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

/*************************END LED STRIP******************/


/**************************UTILS*************************/
String getStrPart(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

/*************************END*UTILS***********************/

String construct_config_packet(device_config deviceConfig) {
    return "SAVE-I:"+String(deviceConfig.intensity)+
            ";M:"+String(deviceConfig._mode)+
            ";C:"+String(deviceConfig.red)+","+String(deviceConfig.green)+","+String(deviceConfig.blue);
}
/***********************ARDUINO*COMMUNICATIONS*****************/
String save_config_to_arduino_status;
boolean save_config_to_arduino_success = true;
String update_device_config_from_arduino_status;
boolean update_device_config_from_arduino_success = true;

String deviceConfigDebugString(device_config _deviceConfig) {
  return "{ ownership=" + String(_deviceConfig.ownership) + ", " +
         "intensity=" + String(_deviceConfig.intensity) + ", " +
         "_mode=" + String(_deviceConfig._mode) + ", " +
         "red=" + String(_deviceConfig.red) + ", " +
         "green=" + String(_deviceConfig.green) + ", " +
         "blue=" + String(_deviceConfig.blue) + ", " +
         "writeCycleCount=" + String(_deviceConfig.eepromWriteCycleCount) + "}";
}

boolean saveConfigToArduino() {
  device_config eepromDeviceConfig;
  EEPROM.get(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, eepromDeviceConfig);

  device_config deviceConfigSavePayload = {
    OWNERSHIP_WRITE_CHECK,
    toSavePayload.intensity,
    toSavePayload._mode,
    toSavePayload.red,
    toSavePayload.green,
    toSavePayload.blue,
    (eepromDeviceConfig.ownership == OWNERSHIP_WRITE_CHECK) ? eepromDeviceConfig.eepromWriteCycleCount + 1 : 1
  };
  Serial.println("Saving " + deviceConfigDebugString(deviceConfigSavePayload));
  EEPROM.put(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, deviceConfigSavePayload);
  EEPROM.commit();
  device_config secondEepromDeviceConfig;
  EEPROM.get(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, secondEepromDeviceConfig);
  Serial.println("After save " + deviceConfigDebugString(secondEepromDeviceConfig));
  //double check data here
  if (secondEepromDeviceConfig.ownership != OWNERSHIP_WRITE_CHECK ||
      secondEepromDeviceConfig.intensity != toSavePayload.intensity ||
      secondEepromDeviceConfig._mode != toSavePayload._mode ||
      secondEepromDeviceConfig.red != toSavePayload.red ||
      secondEepromDeviceConfig.green != toSavePayload.green ||
      secondEepromDeviceConfig.blue != toSavePayload.blue) {
    Serial.println("Problem with saving the device config packet");
    save_config_to_arduino_status = "EEROR-PROBLEM SAVING the device config packet";
    return false;
  }
  save_config_to_arduino_status = "OK";
  return true;
}

boolean update_device_config_from_arduino() {
  Serial.println(F("Pulling device config from EEPROM..."));
  device_config eepromDeviceConfig;
  EEPROM.get(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, eepromDeviceConfig);
  if (eepromDeviceConfig.ownership != OWNERSHIP_WRITE_CHECK) {
    //initialize default
    Serial.println(F("Wrong ownership in EEPROM. Using default device config"));
    eepromDeviceConfig = defaultDeviceConfig;
    deviceConfig = eepromDeviceConfig;
    update_device_config_from_arduino_status = "Wrong ownership in EEPROM. Using default device config";
    return false;
  }
  deviceConfig = eepromDeviceConfig;
  update_device_config_from_arduino_status = "OK";
  return true;
}


/**********************END*ARDUINO*COMMUNICATIONS*****************/

// Use for debugging purposes only
String handle_admin_uart_error = "";
void handleAdminUartCmd() {
  String incommingStr = "";
  boolean stringReady = false;

  while (Serial.available()) {
    incommingStr = Serial.readStringUntil('\n');
    stringReady = true;
  }

  if (stringReady) {
    String adminPart = getStrPart(incommingStr, '+', 0);
    if (adminPart.equals("ADMIN")) {
      String cmd = getStrPart(incommingStr, '+', 1);
    } else {
        handle_admin_uart_error = "Invalid command: " + incommingStr;
    }
  }
}

/************************RESPONSE*PROCESSOR****************/
String processor(const String& var){
  if (var == "INTENSITY") {
    return String(deviceConfig.intensity);
  } else if (var == "_MODE") {
    return String(deviceConfig._mode);
  } else if (var == "RED") {
    return String(deviceConfig.red);
  } else if (var == "GREEN") {
    return String(deviceConfig.green);
  } else if (var == "BLUE") {
    return String(deviceConfig.blue);
  } else if (var == "EEPROM_WRITE_CYCLE_COUNT") {
    return String(deviceConfig.eepromWriteCycleCount);
  } 

  else if (var == "SOFTWARE_VERSION") {
    return String(SOFTWARE_VERSION);
  }
  // Error report
  else if (var == "SAVE_CONFIG_TO_ARDUINO_STATUS") {
    return save_config_to_arduino_success ? "OK" : save_config_to_arduino_status;
  } else if (var == "UPDATE_DEVICE_CONFIG_FROM_ARDUINO_STATUS") {
    return update_device_config_from_arduino_success ? "OK" : update_device_config_from_arduino_status;
  } else if (var == "HANDLE_ADMIN_UART_ERROR") {
    return handle_admin_uart_error;
  } 
  
  else if (var == "NUM_CONNECTED_CLIENT_TO_AP") {
    return String(WiFi.softAPgetStationNum());
  }
  return String();
}
/*******************END*RESPONSE*PROCESSOR******************/

void setupInLowPowerModeAndDisableLED() {
  WiFi.disconnect(true);
  Serial.println(F("Enable lower power mode and disable LED"));
  strip.setBrightness(0);
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0,0,0));
  }
  strip.show();
  
  WiFi.mode(WIFI_OFF);
}

boolean firstUp = false;
void setup() {
  Serial.begin(9600);
  EEPROM.begin(50);

  strip.begin();
  strip.setBrightness(20);
  stableLightMode(strip.Color(255,0,0));
  delay(200);
  stableLightMode(strip.Color(0,0,0));
  delay(200);
  
  
  pinMode(SOFT_SWITCH_POWER_MANAGEMENT_PIN, INPUT_PULLUP);
  Serial.println(F("Enable Access Point mode and enable LED strip"));
  Serial.print(F("Setting AP (Access Point)…"));
  WiFi.persistent(false); 
  Serial.print(F("Setting soft-AP configuration ... "));
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);
  Serial.print(F("SoftAPIP: ")); Serial.println(WiFi.softAPIP());
  Serial.print(F("Local IP: ")); Serial.println(WiFi.localIP());

  Serial.println(F("Setting up SPIFFS"));
  if(!SPIFFS.begin()){
    Serial.println(F("An Error has occurred while mounting SPIFFS"));
    //error here
  }


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("HTTP_GET /");
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("HTTP_POST /save");
    for(int i=0;i<request->params();i++){
      AsyncWebParameter* p = request->getParam(i);
      if (p->name().equals("intensity")) {
        toSavePayload.intensity = p->value().toInt();
      } else if (p->name().equals("_mode")) {
        toSavePayload._mode = p->value().toInt();
      } else if (p->name().equals("red")) {
        toSavePayload.red = p->value().toInt();
      } else if (p->name().equals("green")) {
        toSavePayload.green = p->value().toInt();
      } else if (p->name().equals("blue")) {
        toSavePayload.blue = p->value().toInt();
      }
    }

    dirtySavePacket = true;
    request->send_P(200, "text/plain", "Ok. trying to save packet");
  });
  
  server.begin();
  Serial.println("Done configuring server");
  firstUp = true;
}

#define MAX_CONFIG_REFRESH_RATE 86400000 // 1 days
#define MIN_CONFIG_REFRESH_RATE 100000
#define CHECK_DIRTY_PAYLOAD_PERIOD 2000

unsigned long checkUpdateConfigRefreshTimer = millis();
unsigned long checkDirtyPayloadTimer = millis();
unsigned long checkNumAccessTimer = millis();
unsigned long  config_refresh_rate = MAX_CONFIG_REFRESH_RATE;

byte previousPowerManagerMode = LOW;
void loop() {
  byte currentPowerManagerMode = digitalRead(SOFT_SWITCH_POWER_MANAGEMENT_PIN);
  if (previousPowerManagerMode == LOW && currentPowerManagerMode == LOW && firstUp ) {
    setupInLowPowerModeAndDisableLED();
    firstUp = false;
    previousPowerManagerMode = currentPowerManagerMode;
    return;
  } else if (previousPowerManagerMode == LOW && currentPowerManagerMode == LOW) {
    previousPowerManagerMode = currentPowerManagerMode;
    firstUp = false;
    delay(600);
    return;
    // nothing
  } else if (previousPowerManagerMode == LOW && currentPowerManagerMode == HIGH && !firstUp) {
    Serial.println("Restarting");
    stableLightMode(strip.Color(255,0,0));
    delay(200);
    stableLightMode(strip.Color(0,0,0));
    delay(200);
    resetFunc();
    return;
  } else if (previousPowerManagerMode == HIGH && currentPowerManagerMode == LOW) {
    setupInLowPowerModeAndDisableLED();
    firstUp = false;
    previousPowerManagerMode = currentPowerManagerMode;
    return;
  } else { // previousPowerManagerMode == HIGH && currentPowerManagerMode == HIGH
    // nothing
  }
  previousPowerManagerMode = currentPowerManagerMode;

  update_device_config_from_arduino_success = update_device_config_from_arduino();
  Serial.print(F("Device config: "));
  Serial.println(deviceConfigDebugString(deviceConfig));
  strip.setBrightness(deviceConfig.intensity * 2);
  stableLightMode(strip.Color(0,0,0));
  strip.show();
  checkDirtyPayloadTimer = millis();
  while (1) {
    byte reading = digitalRead(SOFT_SWITCH_POWER_MANAGEMENT_PIN);
    if (reading == LOW) {
      dirtySavePacket = false; 
      resetDeviceMode = false;
      break; 
    }
    // have timer to avoid multiple save at a single time.
    if ((millis() - checkDirtyPayloadTimer) > CHECK_DIRTY_PAYLOAD_PERIOD && dirtySavePacket) {
      save_config_to_arduino_success = saveConfigToArduino();
      dirtySavePacket = false;  
      resetDeviceMode = true;
      checkDirtyPayloadTimer = millis();
    }
    if ((millis() - checkUpdateConfigRefreshTimer) > config_refresh_rate) {
      update_device_config_from_arduino_success = update_device_config_from_arduino();
      checkUpdateConfigRefreshTimer = millis();
    }
    if ((millis() - checkNumAccessTimer) > 2000) {
      int numAccess = WiFi.softAPgetStationNum();
      if (numAccess == 0) {
        config_refresh_rate = MAX_CONFIG_REFRESH_RATE;
      } else {
        config_refresh_rate = MIN_CONFIG_REFRESH_RATE;
      }
      checkNumAccessTimer = millis();
    }
    
//    handleAdminUartCmd();


    if (resetDeviceMode) {
      Serial.println(F("Restarting"));
      resetDeviceMode = false;
      onResetStrip();
      break;
    }
    
    switch (deviceConfig._mode) {
      case STABLE_LIGHT_MODE:
        stableLightMode(getUserConfiguredStripColor()); 
        delay(1000);  
        break;
  
      case LIGHT_SPIN_LIGHT_MODE:
        lightSpin(getUserConfiguredStripColor(), 5, 10000, 40);
        break;
  
      case NIGHT_MODE_LIGHT_MODE:
        nightMode(getUserConfiguredStripColor(), 80, deviceConfig.intensity);
        break;
  
      case RUNING_LIGHT_MODE:
        RunningLights(deviceConfig.red, deviceConfig.green, deviceConfig.blue, 80);
        break;
  
      case RGB_WIPE_LIGHT_MODE:
        if (!colorWipe(strip.Color(255, 0, 0), 50)) { break; }; // Red
        if (!colorWipe(strip.Color(0, 255, 0), 50)) { break; }; // Green
        if (!colorWipe(strip.Color(0, 0, 255), 50)) { break; }; // Blue
        if (!colorWipe(strip.Color(0, 0, 0, 255), 50)) { break; }; // White RGBW
        break;

      case RGB_FADE_IN_AND_OUT: 
        RGBfadeInAndOut();
        break;
  
      case RAINBOW_CYCLE_LIGHT_MODE:
        rainbowCycle(20);
        break;
  
      case SMOOTH_RAINBOW_TRANSITION_LIGHT_MODE:
        smoothRainbowCycle(40);
        break;
  
      case THEATHER_CHASE_RAINBOW_LIGHT_MODE:
        theaterChaseRainbow(50);
        break;
  
      case LIGHT_SHOW_LIGHT_MODE01:
        lightshow01();
        break;
  
      default:
        break;
    }
  }
}



void onResetStrip() {
  resetDeviceMode = false;
  
  // reread mode info here
  Serial.println("onResetStrip....");
}
