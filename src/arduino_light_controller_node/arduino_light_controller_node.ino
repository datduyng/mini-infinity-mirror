/*
 * 1.0.0 Initial
 * 1.0.1 Add software serial
 * 1.0.2 Add light mode and software serial cmd handling
 * 1.1.0 Add LED_STRIP_POWER_PIN GPIO pin to manage LED strip power.
 */
String SOFTWARE_VERSION = "1.1.0";

#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>


#define LED_STRIP_POWER_PIN 7
#define OWNERSHIP_WRITE_CHECK 123
#define DEVICE_CONFIG_EEPROM_PACKET_ADDRESS 0

#define STABLE_LIGHT_MODE 0
#define LIGHT_SPIN_LIGHT_MODE 1
#define NIGHT_MODE_LIGHT_MODE 2
#define GLOW_AND_BLOW_LIGHT_MODE 3

#define RGB_WIPE_LIGHT_MODE 100
#define RAINBOW_CYCLE_LIGHT_MODE 101
#define SMOOTH_RAINBOW_TRANSITION_LIGHT_MODE 102
#define THEATHER_CHASE_RAINBOW_LIGHT_MODE 103
#define LIGHT_SHOW_LIGHT_MODE01 104
#define LIGHT_SHOW_LIGHT_MODE02 105

typedef struct {
  byte ownership;
  byte intensity;
  byte _mode;
  byte red;
  byte green;
  byte blue;
  int  writeCycleCount;
} device_config;



SoftwareSerial mySerial(2,3); // RX, TX

device_config defaultDeviceConfig = {
  OWNERSHIP_WRITE_CHECK,
  50,
  SMOOTH_RAINBOW_TRANSITION_LIGHT_MODE,
  0,
  0,
  250,
  1
};

device_config deviceConfig;

void(* resetFunc) (void) = 0;

#define NEO_PIXEL_PIN 6 // Arduino pin number (most are valid)
#define NUM_PIXEL 30
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXEL, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);


String deviceConfigDebugString(device_config _deviceConfig) {
  return "{ ownership=" + String(_deviceConfig.ownership) + ", " +
         "intensity=" + String(_deviceConfig.intensity) + ", " +
         "_mode=" + String(_deviceConfig._mode) + ", " +
         "red=" + String(_deviceConfig.red) + ", " +
         "green=" + String(_deviceConfig.green) + ", " +
         "blue=" + String(_deviceConfig.blue) + ", " +
         "writeCycleCount=" + String(_deviceConfig.writeCycleCount) + "}";
}

String deviceConfigToPacketString(device_config _deviceConfig) {
  return "DATA-I:" + String(_deviceConfig.intensity) +
         ";M:" + String(_deviceConfig._mode) +
         ";C:" + String(_deviceConfig.red) + "," + String(_deviceConfig.green) + "," + String(_deviceConfig.blue) +
         ";W:" + String(_deviceConfig.writeCycleCount);
}

void sendNodeMCU(String str) {
  mySerial.println(str);
}

void setup() {

  Serial.println("Initializing mini infinity mirror software (Version: " + SOFTWARE_VERSION + ")");
  // put your setup code here, to run once:
  Serial.begin(9600);
  mySerial.begin(19200);
  Serial.println(F("Starting Arduino"));
  Serial.println(F("Configuring LED strip..."));
  strip.begin();
  strip.setBrightness(deviceConfig.intensity * 2);
  stableLightMode(strip.Color(0, 0, 255));
  delay(200);
  stableLightMode(strip.Color(0, 0, 0));
  delay(200);
  
  Serial.println(F("Pulling device config from EEPROM...."));
  device_config eepromDeviceConfig;
  EEPROM.get(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, eepromDeviceConfig);
  if (eepromDeviceConfig.ownership != OWNERSHIP_WRITE_CHECK) {
    //initialize default
    Serial.println(F("Wrong ownership in EEPROM. Using default device config"));
    eepromDeviceConfig = defaultDeviceConfig;
  }
  deviceConfig = eepromDeviceConfig;
  Serial.println("Device config: " + deviceConfigDebugString(deviceConfig));

  pinMode(LED_STRIP_POWER_PIN, INPUT);
  Serial.println(F("Done intializing."));

}

void loop() {
  Serial.print("LED val");
  Serial.println(digitalRead(LED_STRIP_POWER_PIN));
  if (digitalRead(LED_STRIP_POWER_PIN) == HIGH) {
    Serial.println("lighting");
    ESPUartListener();
    delay(400);// some slow universal loop delay. else SoftwareSerial will fail.
    switch (deviceConfig._mode) {
      case STABLE_LIGHT_MODE:
        stableLightMode(getUserConfiguredStripColor());   
        break;
  
      case LIGHT_SPIN_LIGHT_MODE:
        lightSpin(getUserConfiguredStripColor(), 5, 10000, 40);
        break;
  
      case NIGHT_MODE_LIGHT_MODE:
        nightMode(getUserConfiguredStripColor(), 80, deviceConfig.intensity);
        break;
  
      case GLOW_AND_BLOW_LIGHT_MODE:
        break;
  
      case RGB_WIPE_LIGHT_MODE:
        colorWipe(strip.Color(255, 0, 0), 50); // Red
        colorWipe(strip.Color(0, 255, 0), 50); // Green
        colorWipe(strip.Color(0, 0, 255), 50); // Blue
        colorWipe(strip.Color(0, 0, 0, 255), 50); // White RGBW
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
  
      case LIGHT_SHOW_LIGHT_MODE02:
        break;
  
      default:
        break;
    }
  }
}


void stableLightMode(uint32_t color) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void ESPUartListener() {
  if (digitalRead(LED_STRIP_POWER_PIN) == LOW) {
    resetFunc();
    return;
  }
  String incommingStr = "";
  boolean stringReady = false;

  while (mySerial.available()) {
    incommingStr = mySerial.readString();
    stringReady = true;
  }

  if (stringReady) {
    Serial.println("From ESP: [" + incommingStr + "]");
    String cmd = getStrPart(incommingStr, '-', 0);
    if (cmd.equals("SAVE")) {
      String dataPacket = getStrPart(incommingStr, '-', 1);
      // I:100;M:0;C:123,24,12
      byte intensity = getStrPart(getStrPart(dataPacket, ';', 0), ':', 1).toInt();
      byte _mode = getStrPart(getStrPart(dataPacket, ';', 1), ':', 1).toInt();
      String rgbColorStr = getStrPart(getStrPart(dataPacket, ';', 2), ':', 1);
      byte redVal = getStrPart(rgbColorStr, ',', 0).toInt();
      byte greenVal = getStrPart(rgbColorStr, ',', 1).toInt();
      byte blueVal = getStrPart(rgbColorStr, ',', 2).toInt();
      device_config eepromDeviceConfig;
      EEPROM.get(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, eepromDeviceConfig);

      device_config deviceConfigSavePayload = {
        OWNERSHIP_WRITE_CHECK,
        intensity,
        _mode,
        redVal,
        greenVal,
        blueVal,
        (eepromDeviceConfig.ownership == OWNERSHIP_WRITE_CHECK) ? eepromDeviceConfig.writeCycleCount + 1 : 1
      };

      Serial.println("Saving " + deviceConfigDebugString(deviceConfigSavePayload) + " From ESP");
      EEPROM.put(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, deviceConfigSavePayload);

      device_config secondEepromDeviceConfig;
      EEPROM.get(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, secondEepromDeviceConfig);
      //double check data here
      if (secondEepromDeviceConfig.ownership != OWNERSHIP_WRITE_CHECK ||
          secondEepromDeviceConfig.intensity != intensity ||
          secondEepromDeviceConfig._mode != _mode ||
          secondEepromDeviceConfig.red != redVal ||
          secondEepromDeviceConfig.green != greenVal ||
          secondEepromDeviceConfig.blue != blueVal) {
        Serial.println("Problem with saving the device config packet");
        sendNodeMCU("EEROR-PROBLEM SAVING the device config packet");
      } else {
        sendNodeMCU("OK-");
      }
      //      delay(10000);
      Serial.println("Resetting with new config");
      delay(200);//unsure why this delay is needed. resetFunc won't work correctly without
      resetFunc();
    } else if (cmd.equals("GET_CONFIG")) {
      device_config eepromDeviceConfig;
      EEPROM.get(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, eepromDeviceConfig);
      sendNodeMCU(deviceConfigToPacketString(eepromDeviceConfig));
      Serial.println(deviceConfigToPacketString(eepromDeviceConfig));
    } else {
      sendNodeMCU("INVALID_COMMAND-" + incommingStr);
      Serial.println("SEND: INVALID_COMMAND-" + String(incommingStr));
    }
  }
}


/********************LED STRIP ****************/
uint32_t getUserConfiguredStripColor() {
  return strip.Color(deviceConfig.red, deviceConfig.green, deviceConfig.blue);
}

void lightshow01() {
  strip.setBrightness(80);
  colorWipe(strip.Color(255, 0, 0), 50); // Red
  colorWipe(strip.Color(0, 255, 0), 50); // Green
  colorWipe(strip.Color(0, 0, 255), 50); // Blue
  colorWipe(strip.Color(0, 0, 0, 255), 50); // White RGBW
  // Send a theater pixel chase in...
  theaterChase(strip.Color(127, 127, 127), 50); // White
  theaterChase(strip.Color(127, 0, 0), 50); // Red
  theaterChase(strip.Color(0, 0, 127), 50); // Blue
  rainbowCycle(10);
  smoothRainbowCycle(10);
  theaterChaseRainbow(30);
  lightSpin(strip.Color(0, 255, 0), 5, 00, 40);
  nightMode(strip.Color(0, 255, 0), 20, 50);
}

void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();
      ESPUartListener();
      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
      ESPUartListener();
    }
  }
}

void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
    ESPUartListener();
  }
}

void lightSpin(uint32_t color, uint8_t group, uint16_t numSteps, uint8_t wait) {
  if (group >= strip.numPixels()) {
//    Serial.println("ERROR: lightSpin() group need to be less than numPixels");
    return;
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
    ESPUartListener();
  }
}

void nightMode(uint32_t color, uint8_t wait, uint8_t top) {
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
    ESPUartListener();
  }
  delay(700);
  ESPUartListener();
  delay(700);
  ESPUartListener();
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
    ESPUartListener();
  }
}

void smoothRainbowCycle(uint8_t wait) {
  uint16_t i, j;
  Serial.println("Start rainbow");
  
  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
    ESPUartListener();
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
    ESPUartListener();
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();
      ESPUartListener();
      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
    ESPUartListener();
  }
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

/********************LED STRIP*************/


String getStrPart(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
