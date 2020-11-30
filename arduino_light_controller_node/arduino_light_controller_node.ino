/*
 * 
 */
String SOFTWARE_VERSION = "1.0.0";

#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

SoftwareSerial mySerial(11, 12); // RX, TX
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
#define NUM_PIXEL 29
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
    return "SAVE-I:"+String(_deviceConfig.intensity)+
            ";M:"+String(_deviceConfig._mode)+
            ";C:"+String(_deviceConfig.red)+","+String(_deviceConfig.green)+","+String(_deviceConfig.blue) +
            ";W:"+String(_deviceConfig.writeCycleCount);
}

void setup() {
  Serial.println("Initializing mini infinity mirror software (Version: " + SOFTWARE_VERSION + ")");
  // put your setup code here, to run once:
  Serial.begin(9600);
  mySerial.begin(9600);

  Serial.println("Pulling device config from EEPROM....");
  device_config eepromDeviceConfig;
  EEPROM.get(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, eepromDeviceConfig);
  if (eepromDeviceConfig.ownership != OWNERSHIP_WRITE_CHECK) {
    //initialize default
    Serial.println("Wrong ownership in EEPROM. Using default device config");
    eepromDeviceConfig = defaultDeviceConfig;
  }
  deviceConfig = eepromDeviceConfig;
  Serial.println("Device config: " + deviceConfigDebugString(deviceConfig));

  Serial.println("Configuring strip...");
  strip.begin();
  strip.setBrightness(deviceConfig.intensity*2);
  strip.show();
  
  Serial.println("Done intializing.");
}



void ESPUartListener() {
  String incommingStr = "";
  boolean stringReady = false;

  while (mySerial.available()) {
    incommingStr = mySerial.readString();
    stringReady = true;
  }

  if (stringReady) {
    Serial.println("From ESP: "+incommingStr);
    String cmd = getStrPart(incommingStr, '-', 0);
    Serial.println("From ESP COMMAND:" + cmd);
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
            mySerial.println("EEROR-PROBLEM SAVING the device config packet");
      } else {
        mySerial.println("OK-");
      }
//      delay(10000);
      Serial.println("reset");
      resetFunc();
    } else if (cmd.equals("GET_CONFIG-")) {
      device_config eepromDeviceConfig;
      EEPROM.get(DEVICE_CONFIG_EEPROM_PACKET_ADDRESS, eepromDeviceConfig);
      mySerial.println("DATA-"+deviceConfigToPacketString(eepromDeviceConfig));
      Serial.println("DATA-I:100;M:0;C:123,24,12");
    } else {
      mySerial.println("INVALID_COMMAND-"+incommingStr);
      Serial.println("SEND: INVALID_COMMAND-"+String(incommingStr));
    }
  }
}

void loop() {
  ESPUartListener();
  switch (deviceConfig._mode) {
    case STABLE_LIGHT_MODE:
    #define GROUP_
      for (uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, getUserConfiguredStripColor());
        strip.show();
      }
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
      break;

    case RAINBOW_CYCLE_LIGHT_MODE:    
      break;

    case SMOOTH_RAINBOW_TRANSITION_LIGHT_MODE:    
      break;

    case THEATHER_CHASE_RAINBOW_LIGHT_MODE:    
      break;

    case LIGHT_SHOW_LIGHT_MODE01:    
      break;

    case LIGHT_SHOW_LIGHT_MODE02:    
      break;
      
    default:
      break;
  }
}


/********************LED STRIP ****************/
uint32_t getUserConfiguredStripColor() {
  return strip.Color(deviceConfig.red, deviceConfig.green, deviceConfig.blue);
}

void lightSpin(uint32_t color, uint8_t group, uint16_t numSteps, uint8_t wait) {
  if (group >= strip.numPixels()) {
    Serial.println("ERROR: lightSpin() group need to be less than numPixels");
    return;
  }
  for (uint16_t i=0; i<group; i++) {
    strip.setPixelColor(i, color);
    strip.show();
  }

  for (int i=group; i<numSteps; i++) {
    strip.setPixelColor(i%strip.numPixels(), color);
    strip.setPixelColor((i-group)%strip.numPixels(), strip.Color(0, 0, 0));
    strip.show();
    delay(wait);
    ESPUartListener();
  }
}

void nightMode(uint32_t color, uint8_t wait, uint8_t top) {
  for (int i=0; i<=top; i++) {
    strip.setBrightness(i);
    for(uint16_t j=0; j<strip.numPixels(); j++) {
      strip.setPixelColor(j, color);
    }
    strip.show();
    if (i < 40) {
      delay(wait);
    }
    delay(wait);
    ESPUartListener();
  }

  delay(1400);
  ESPUartListener();
  for (int i=top; i>=0; i--) {
    strip.setBrightness(i);
    for(uint16_t j=0; j<strip.numPixels(); j++) {
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
/********************LED STRIP*************/


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
