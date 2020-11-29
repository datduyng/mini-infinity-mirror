#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SoftwareSerial.h>

const char* ssid     = "MindYourOwnBusiness";
const char* password = "123456789";

AsyncWebServer server(80);
SoftwareSerial mySerial(5,4); // RX, TX

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
    }
    .para-spacing {
      margin-left: 20px;
    }
    input[type='radio'] { transform: scale(2); margin-right: 10px }
  </style>
</head>
<body>
  <div style="margin: auto; width: 375px;">
    <h2 style="text-align: center;">Mini infinity mirror control</h2>

    <h4>General setting</h4>
    <div class="para-spacing">
      <label for="intensity">Intensity:</label>
      <input type="number" id="intensity" name="intensity" min="10" max="100" placeholder="10 - 100" style="width: 80px;">
    </div>



    <h4>Mode setting</h4>
    <div class="para-spacing">
      <label for="head">Color: (only apply for 4 options below)</label>
      <input type="color" value="#e66465">
      <div class="para-spacing">
        <input type="radio" id="male" name="gender" value="nightmode">
        <label for="male">Night mode</label><br><br>
        <input type="radio" id="female" name="gender" value="hopefulmode">
        <label for="female">Hopeful</label><br><br>
        <input type="radio" id="other" name="gender" value="lightshow01">
        <label for="other">Light show 1</label><br><br>
        <input type="radio" id="other" name="gender" value="lightshow02">
        <label for="other">Light show 2</label><br><br>
      </div>
      <input type="radio" id="other" name="gender" value="rainbowshow">
      <label for="other">Rainbow show</label>
    </div>
    <br>
    <br>
    <button onclick="save_config()">Save</button>
  </div>
  <script>

    function save_config() {
      fetch("/save", {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        }, 
        body: new URLSearchParams({
            intensity: 90,
            _model: 1
        })
      });
  </script>
</body>
</html>
)rawliteral";

String processor(const String& var){
  return String();
}

String tprocessor(const String& var){
  if (var == "ERROR") {
    return String("ERROR TESTING");
  }
  return String();
}

IPAddress local_IP(1,2,3,4);
IPAddress gateway(1,2,3,1);
IPAddress subnet(255,255,255,0);


typedef struct {
  int intensity;
  int _mode;
  int redColor;
  int greenColor;
  int blueColor;  
} device_config;

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
void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  Serial.print("Setting AP (Access Point)…");


  WiFi.persistent( false ); 
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    device_config deviceConfig;
    deviceConfig.intensity = 0;
    deviceConfig._mode = 0;
    deviceConfig.redColor = 0;
    deviceConfig.greenColor = 0;
    deviceConfig.blueColor = 0;
    Serial.println("params " + String(request->params()));
    for(int i=0;i<request->params();i++){
      
      AsyncWebParameter* p = request->getParam(i);
      Serial.println("Processing " + String(p->name()));
      if (p->name().equals("intensity")) {
        deviceConfig.intensity = p->value().toInt();
      } else if (p->name().equals("_mode")) {
        deviceConfig._mode = p->value().toInt();
      } else if (p->name().equals("redColor")) {
        deviceConfig.redColor = p->value().toInt();
      } else if (p->name().equals("greenColor")) {
        deviceConfig.greenColor = p->value().toInt();
      } else if (p->name().equals("blueColor")) {
        deviceConfig.blueColor = p->value().toInt();
      }
    }

    mySerial.println("SAVE-I:"+String(deviceConfig.intensity)+",M:"+String(deviceConfig._mode)+",C:"+String(deviceConfig.redColor)+","+String(deviceConfig.greenColor)+","+String(deviceConfig.blueColor));
    startSaveRequestTimer = millis();
    Serial.println("Received a request");
    unsigned long currentTime = millis();
    #define TIMEOUT 5000
    do {
      Serial.println("Waiting for Arduino");
      if ((currentTime - startSaveRequestTimer) > TIMEOUT) {
        Serial.println("timout");
        request->send_P(500, "text/plain", "Some unexpected error is going on with the UNO. Contact Dom!!");
        return;
      }

      String cmd = "";
      Serial.println("here");
      if (mySerial.available()) {
        Serial.println("read string");
        String packet = mySerial.readStringUntil('\n');
        Serial.println("received a packet" + String(packet));
        cmd = getStrPart(packet, '-', 0);
        if (cmd.equals("OK")) {
          request->send_P(200, "text/plain", "OK");
          return;
        } else {
          request->send_P(500, "text/plain", "Unexpected packet. %ERROR%", tprocessor);
          Serial.println("Unexpected packet. "+packet);
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
  delay(2000);
}
