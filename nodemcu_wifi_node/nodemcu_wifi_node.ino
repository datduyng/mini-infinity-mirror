/*
 * 
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SoftwareSerial.h>

//#define _stackSize 7000/4

String SOFTWARE_VERSION = "1.0.0";

const char* ssid     = "LL Mini Infinity Mirror";
const char* password = "123456789";

AsyncWebServer server(80);
SoftwareSerial mySerial(5,4); // RX, TX

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

//const char index_html[] PROGMEM = "<h1>test</h1>";
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style>
      html {
        font-family: Arial;
        display: inline-block;
        margin: 0px auto;
      }
      .para-spacing {
        margin-left: 20px;
      }
      input[type="radio"] {
        transform: scale(2);
        margin-right: 10px;
      }
    </style>
  </head>
  <body>
    <div style="margin: auto; width: 375px">
      <h2 style="text-align: center">Mini infinity mirror control</h2>

      <h4>General setting</h4>
      <div class="para-spacing">
        <label for="intensity">Intensity:     5</label>
        <input type="range" min="5" max="120" id="intensity-slider" /> 
        <label for="intensity">120</label>
      </div>

      <h4>Mode setting</h4>
      <div class="para-spacing">
        <label for="head">Color: (only apply for 4 options below)</label>
        <input type="color" value="#e66465" id="color-picker"/>
        <div class="para-spacing">
          <br />
          <input type="radio" id="lightmode-0" name="lightmode" value="0" />
          <label for="male">Stable</label><br /><br />
          <input type="radio" id="lightmode-1" name="lightmode" value="1" />
          <label for="female">Light spin</label><br /><br />
          <input type="radio" id="lightmode-2" name="lightmode" value="2" />
          <label for="other">Night mode</label><br /><br />
          <input type="radio" id="lightmode-3" name="lightmode" value="3" />
          <label for="other">Glow and blow</label><br /><br />
        </div>
        <input type="radio" id="lightmode-100" name="lightmode" value="100" />
        <label for="other">RGB swipe</label><br /><br />
        <input type="radio" id="lightmode-101" name="lightmode" value="101" />
        <label for="other">Rainbow cycle</label><br /><br />
        <input type="radio" id="lightmode-102" name="lightmode" value="102" />
        <label for="other">Smooth rainbow</label><br /><br />
        <input type="radio" id="lightmode-103" name="lightmode" value="103" />
        <label for="other">Theater chase rainbow show</label><br /><br />
        <input type="radio" id="lightmode-104" name="lightmode" value="104" />
        <label for="other">Light show 01</label><br /><br />
        <input type="radio" id="lightmode-105" name="lightmode" value="105" />
        <label for="other">Light show 02</label><br /><br />
      </div>
      <br />
      <br />
      <button onclick="save_config()">Save</button>

      <button onclick="populateDefault()">Reset</button>

      <br />

      <br />
      <button onclick="populateDebugMessage()">vvv Generate debug stats for nerd vvvv</button>
      <div id="debug-area">

      </div>
    </div>
    <script>
      // let userConfig = {
      //     intensity: %INTENSITY%,
      //     _mode: %_MODE%,
      //     red: %RED%,
      //     blue: %BLUE%,
      //     green: %GREEN%,
      // };

      // let debug = {
      //   eepromWriteCycleCount: %EEPROM_WRITE_CYCLE_COUNT%,
      //   last_error: %MOST_RECENT_ERROR%,
      //   num_connected_client_to_access_point: %NUM_CONNECTED_CLIENT_TO_AP%
      // }
    var userConfig = {
      intensity: 100,
      _mode: 2,
      red: 0,
      blue: 0,
      green: 255,
    };

    // var debug = {
    //   eepromWriteCycleCount: 100,
    //   last_error: "Test",
    //   num_connected_client_to_access_point: 1
    // };
    // var default_config = {
    //     intensity: 50,
    //     _mode: 102,
    //     red: 0,
    //     blue: 250,
    //     green: 0,
    // };
    
    function hexToRgb(hex) {
      var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
      return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16)
      } : null;
    }
    function componentToHex(c) {
      var hex = c.toString(16);
      return hex.length == 1 ? "0" + hex : hex;
    }

    function rgbToHex(r, g, b) {
      return "#" + componentToHex(r) + componentToHex(g) + componentToHex(b);
    }

    function populateDefault() {
      document.getElementById("lightmode-"+default_config._mode).checked = true;
      document.getElementById("intensity-slider").value = default_config.intensity;
      document.getElementById("color-picker").value = rgbToHex(default_config.red, default_config.green, default_config.blue);
    }

    function populateUserconfig() {
      document.getElementById("lightmode-"+userConfig._mode).checked = true;
      document.getElementById("intensity-slider").value = userConfig.intensity;
      document.getElementById("color-picker").value = rgbToHex(userConfig.red, userConfig.green, userConfig.blue);
    }
    populateUserconfig();

    function populateDebugMessage() {
      document.getElementById("debug-area").innerHTML = 
      `
        <pre>
User config: ${JSON.stringify(userConfig, undefined, 2)}
Debug: ${JSON.stringify(debug, undefined, 2)}
        </pre>
      `
    }

    function save_config() {
      let rgb = hexToRgb(document.getElementById("color-picker").value);

      fetch("http://192.168.1.1/save", {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: new URLSearchParams({
            intensity: parseInt(document.getElementById("intensity-slider").value),
            _mode: document.querySelector('input[name="lightmode"]:checked').value,
            red: rgb.r,
            green: rgb.g,
            blue: rgb.b 
        })
      });
    }
  </script>
  </body>
</html>

)rawliteral";

String arduinoErrorRequestError = "No error";

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
  } else if (var == "MOST_RECENT_ERROR") {
    return arduinoErrorRequestError;
  } else if (var == "NUM_CONNECTED_CLIENT_TO_AP") {
    return String(WiFi.softAPgetStationNum());
  }
  return String();
}
String error_message_processor(const String& var){
  if (var == "ERROR") {
    return arduinoErrorRequestError;
  }
  return String();
}

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

unsigned long startSaveRequestTimer;

String construct_config_packet(device_config deviceConfig) {
    return "SAVE-I:"+String(deviceConfig.intensity)+
            ";M:"+String(deviceConfig._mode)+
            ";C:"+String(deviceConfig.red)+","+String(deviceConfig.green)+","+String(deviceConfig.blue);
}

/*
 * 
 */
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
//      Serial.println(F("Executing command: " + incommingStr));
      if (cmd.equals("SEND_NANO_TEST_CONFIG_PAYLOAD")) {
        // ADMIN+SEND_NANO_TEST_CONFIG_PAYLOAD
        device_config deviceConfig = { 88, 2, 1, 2, 3, 0 };
        mySerial.println(construct_config_packet(deviceConfig));
//        Serial.println("Sent " + construct_config_packet(deviceConfig));
      } else if (cmd.equals("NANO")) {
        // ADMIN+NANO+SAVE-I:33;M:1;C:4,2,3
        // ADMIN+NANO+GET_CONFIG-
        String data = getStrPart(incommingStr, '+', 2);
        mySerial.println(data);
//        Serial.println("Sent to Nano: " + data);
      } else {
//        Serial.println("Invalid admin command: " + cmd);
      }
    } else {
//      Serial.println("Invalid command: " + incommingStr);
    }
  }
}

void ArduinoUartListener() {
  String incommingStr = "";
  boolean stringReady = false;

  while (mySerial.available()) {
    incommingStr = mySerial.readString();
    stringReady = true;
  }

  if (stringReady) { 
    Serial.println("FROM Arduino: " + incommingStr);
  }
}

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
//  Serial.print("Setting AP (Access Point)…");
  
  WiFi.persistent(false); 
//  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
//  Serial.print("AP IP address: ");
//  Serial.println(IP);

  // Print ESP8266 Local IP Address
//  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){

    mySerial.println("GET_CONFIG-");

    startSaveRequestTimer = millis();
    #define TIMEOUT 5000
    unsigned long currentTime = millis();
////    Serial.println("Here");
    do {
      Serial.println("Waiting for Arduino to GET_CONFIG");
      if ((currentTime - startSaveRequestTimer) > TIMEOUT) {
        Serial.println("time out");
        arduinoErrorRequestError = "Timout while requesting for config from Arduino";
//        Serial.println(arduinoErrorRequestError);
        request->send_P(200, "text/html", index_html, processor);
        return;
      }
      Serial.println("Here1");
      String cmd = "";
      if (mySerial.available()) {
      
        Serial.println("Before read");
//        Serial.println("BSSL stack: " + String(stack_thunk_get_max_usage()));
        String packet = mySerial.readStringUntil('\n');
         Serial.println("Done reading " + packet);
//        Serial.println("After read");
//        Serial.println("Here2 " + String(packet));
//        Serial.println("Received a packet: " + String(packet));
        cmd = getStrPart(packet, '-', 0);
        if (cmd.equals("DATA")) {
          String dataPacket = getStrPart(packet, '-', 1);
          String("DATA " + String(dataPacket));
          // I:100;M:0;C:123,24,12;W:123
          byte intensity = getStrPart(getStrPart(dataPacket, ';', 0), ':', 1).toInt();
          byte _mode = getStrPart(getStrPart(dataPacket, ';', 1), ':', 1).toInt();
          String rgbColorStr = getStrPart(getStrPart(dataPacket, ';', 2), ':', 1);
          byte redVal = getStrPart(rgbColorStr, ',', 0).toInt();
          byte greenVal = getStrPart(rgbColorStr, ',', 1).toInt();
          byte blueVal = getStrPart(rgbColorStr, ',', 2).toInt();
          byte eepromWriteCycleCount = getStrPart(getStrPart(dataPacket, ';', 3), ':', 1).toInt();
      
          deviceConfig = { intensity, _mode, redVal, greenVal, blueVal, eepromWriteCycleCount };
          request->send_P(200, "text/html", index_html, processor);
          return;
        } else {
          arduinoErrorRequestError = String(packet);
//          Serial.println(arduinoErrorRequestError);
          request->send_P(200, "text/html", index_html, processor);
          return;
        }
      }
      
      currentTime = millis();
        
    } while(!mySerial.available());
    arduinoErrorRequestError = "Error: didn't received config info from Arduino";
    request->send_P(200, "text/html", index_html, processor);
  });
  
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    device_config deviceConfig;
    deviceConfig.intensity = 0;
    deviceConfig._mode = 0;
    deviceConfig.red = 0;
    deviceConfig.green = 0;
    deviceConfig.blue = 0;
    for(int i=0;i<request->params();i++){
      
      AsyncWebParameter* p = request->getParam(i);
      if (p->name().equals("intensity")) {
        deviceConfig.intensity = p->value().toInt();
      } else if (p->name().equals("_mode")) {
        deviceConfig._mode = p->value().toInt();
      } else if (p->name().equals("red")) {
        deviceConfig.red = p->value().toInt();
      } else if (p->name().equals("green")) {
        deviceConfig.green = p->value().toInt();
      } else if (p->name().equals("blue")) {
        deviceConfig.blue = p->value().toInt();
      }
    }

    mySerial.println(construct_config_packet(deviceConfig));
    startSaveRequestTimer = millis();
    #define TIMEOUT 5000
    unsigned long currentTime = millis();
    do {
//      Serial.println("Waiting for Arduino");
      if ((currentTime - startSaveRequestTimer) > TIMEOUT) {
//        Serial.println("timout");
        request->send_P(500, "text/plain", "Some unexpected error is going on with the UNO. Contact Dom!!");
        return;
      }

      String cmd = "";
      if (mySerial.available()) {
        String packet = mySerial.readStringUntil('\n');
//        Serial.println("Received a packet: " + String(packet));
        cmd = getStrPart(packet, '-', 0);
        if (cmd.equals("OK")) {
          request->send_P(200, "text/plain", "OK");
          return;
        } else {
          arduinoErrorRequestError = String(packet);
          request->send_P(500, "text/plain", "Unexpected packet. %ERROR%", error_message_processor);
//          Serial.println("Unexpected packet. "+packet);
          return;
        }
      }
      
      currentTime = millis();
    } while(!mySerial.available());

    request->send_P(500, "text/plain", "Unknown unexpected error. Contact Dom!!");
  });
  
  server.begin();
}

void loop() {
  Serial.println("Alive");
  delay(1000);
}
