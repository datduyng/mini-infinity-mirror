/*
 * 1.0.0 Initial
 * 1.0.1 Remove software serial with hardware serial
 * 1.0.2 Remove heavy config saving and requesting process out of AsyncRequest context
 * 1.0.3 Reuse software serial + add SPIFFS
 * 1.0.4 Fix bug device_config deepCopy return null bug ;(
 * 1.1.0 When there are nodes connected to this AP --> decrease the config refresh rate to boost user experience
 * 1.1.1 Auto scaling config refresh rate when there are no connected devices and there are.
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(5,4); // RX, TX

const char* SOFTWARE_VERSION = "1.1.1";
const char* ssid     = "LL Mini Infinity Mirror";
const char* password = "123456789";

AsyncWebServer server(80);

IPAddress local_IP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

typedef struct {
  byte intensity;
  byte _mode;
  byte red;
  byte green;
  byte blue;  
  int eepromWriteCycleCount;
} device_config __attribute__((packed));
device_config deviceConfig;
device_config deepCopy(device_config other) {
  device_config thisConfig;
  thisConfig.intensity = other.intensity;
  thisConfig._mode = other._mode;
  thisConfig.red = other.red;
  thisConfig.green = other.green;
  thisConfig.blue = other.blue;
  return thisConfig;
}

device_config toSavePayload;
boolean dirtySavePacket = false;

String arduinoErrorRequestError = "No error";

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

void sendArduino(String str) {
  mySerial.println(str);
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
boolean saveConfigToArduino() {
  Serial.print(F("Demanding Arduino to save packet: "));
  Serial.println(construct_config_packet(toSavePayload));
  sendArduino(construct_config_packet(toSavePayload));
  unsigned long startRequestTimer = millis();
  #define TIMEOUT 5000
  unsigned long currentTime = millis();
  do {
    if ((currentTime - startRequestTimer) > TIMEOUT) {
      Serial.println("TIMEOUT");
      save_config_to_arduino_status = "Timeout while saving config";
      return false;
    }

    String cmd = "";
    if (mySerial.available()) {
      String packet = mySerial.readStringUntil('\n');
      Serial.println("Received a packet: " + String(packet));
      cmd = getStrPart(packet, '-', 0);
      if (cmd.equals("OK")) {
        save_config_to_arduino_status = "OK ";
        deviceConfig = deepCopy(toSavePayload);
        return true;
      } else {
        save_config_to_arduino_status = "Unexpected packet. " + String(packet);
        return false;
      }
    }
    currentTime = millis();
  } while(!mySerial.available());
  save_config_to_arduino_status = "Unexpected error.";
  return false;
}

boolean update_device_config_from_arduino(uint16_t timeout) {
  sendArduino("GET_CONFIG-");
  unsigned long startRequestTimer = millis();
  unsigned long currentTime = millis();
  do {
    if ((currentTime - startRequestTimer) > timeout) {
      Serial.println(F("TIMEOUT"));
      update_device_config_from_arduino_status = "Timout while requesting for config from Arduino";
      return false;
    }
    String cmd = "";
    Serial.println(F("Requesting"));
    if (mySerial.available()) {
      String packet = mySerial.readStringUntil('\n');
      Serial.println("Received: " + String(packet));
      cmd = getStrPart(packet, '-', 0);
      if (cmd.equals("DATA")) {
        String dataPacket = getStrPart(packet, '-', 1);
        // I:100;M:0;C:123,24,12;W:123
        byte intensity = getStrPart(getStrPart(dataPacket, ';', 0), ':', 1).toInt();
        byte _mode = getStrPart(getStrPart(dataPacket, ';', 1), ':', 1).toInt();
        String rgbColorStr = getStrPart(getStrPart(dataPacket, ';', 2), ':', 1);
        byte redVal = getStrPart(rgbColorStr, ',', 0).toInt();
        byte greenVal = getStrPart(rgbColorStr, ',', 1).toInt();
        byte blueVal = getStrPart(rgbColorStr, ',', 2).toInt();
        byte eepromWriteCycleCount = getStrPart(getStrPart(dataPacket, ';', 3), ':', 1).toInt();
        
        deviceConfig = { intensity, _mode, redVal, greenVal, blueVal, eepromWriteCycleCount };
        update_device_config_from_arduino_status = "success"; 
        return true;
      } else {
        update_device_config_from_arduino_status = "Invalid data payload "+ String(packet);
        return false;
      }
    }
    
    currentTime = millis();
      
  } while(!mySerial.available());
  update_device_config_from_arduino_status = "Unexpected error.";
  return false;
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
      if (cmd.equals("SEND_NANO_TEST_CONFIG_PAYLOAD")) {
        // ADMIN+SEND_NANO_TEST_CONFIG_PAYLOAD
        device_config deviceConfig = { 88, 2, 1, 2, 3, 0 };
        sendArduino(construct_config_packet(deviceConfig));
      } else if (cmd.equals("NANO")) {
        // ADMIN+NANO+SAVE-I:33;M:1;C:4,2,3
        // ADMIN+NANO+GET_CONFIG-
        String data = getStrPart(incommingStr, '+', 2);
        sendArduino(data);
      } else {
        handle_admin_uart_error = "Invalid admin command: " + cmd;
      }
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

void setup() {
  Serial.begin(9600);
  mySerial.begin(19200);
  Serial.print("Setting AP (Access Point)…");
  WiFi.persistent(false); 
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print(F("AP IP address: "));
  Serial.println(IP);
  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());

  Serial.println("Waiting for Arduino to wake up...");
  delay(2000);
  update_device_config_from_arduino_success = update_device_config_from_arduino(4000);
  Serial.print(F("Device config: "));
  Serial.println(construct_config_packet(deviceConfig));

  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
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
}

#define MAX_CONFIG_REFRESH_RATE 86400000 // 1 days
#define MIN_CONFIG_REFRESH_RATE 100000
#define CHECK_DIRTY_PAYLOAD_PERIOD 2000

unsigned long checkUpdateConfigRefreshTimer = millis();
unsigned long checkDirtyPayloadTimer = millis();
unsigned long checkNumAccessTimer = millis();
unsigned long  config_refresh_rate = MAX_CONFIG_REFRESH_RATE;

void loop() {
  // have timer to avoid multiple save at a single time.
  if ((millis() - checkDirtyPayloadTimer) > CHECK_DIRTY_PAYLOAD_PERIOD && dirtySavePacket) {
    save_config_to_arduino_success = saveConfigToArduino();
    dirtySavePacket = false;  
    checkDirtyPayloadTimer = millis();
  }
  
  if ((millis() - checkUpdateConfigRefreshTimer) > config_refresh_rate) {
    update_device_config_from_arduino_success = update_device_config_from_arduino(2000);
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
  
  handleAdminUartCmd();
}
