#include <SoftwareSerial.h>

/*
 * 
 * payload:
 * ESP --> arduino
 * SAVE-I:100;M:0;C:123,24,12 (OK)
 * GET_CONFIG- (100;M:0,C:123,24,12)
 * 
 * Arduino --> ESP
 * 
 * 
 */
SoftwareSerial mySerial(2,3); // RX, TX
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  mySerial.begin(9600);
  delay(1000);
  Serial.println("start");
}

void ESPUartListener() {
  String incommingStr = "";
  boolean stringReady = false;

  while (mySerial.available()) {
    incommingStr = mySerial.readString();
    stringReady = true;
  }

  if (stringReady) {
    Serial.println("Received: "+incommingStr);
    String cmd = getStrPart(incommingStr, '-', 0);
    Serial.println("COMMAND:" + cmd);
    if (cmd.equals("SAVE")) {
      String dataPacket = getStrPart(incommingStr, '-', 1);
      // I:100;M:0;C:123,24,12
      int intensity = getStrPart(getStrPart(dataPacket, ';', 0), ':', 1).toInt();
      int _mode = getStrPart(getStrPart(dataPacket, ';', 1), ':', 1).toInt();
      String rgbColorStr = getStrPart(dataPacket, ';', 2);
      int redVal = getStrPart(rgbColorStr, ',', 0).toInt();
      int greenVal = getStrPart(rgbColorStr, ',', 1).toInt();
      int blueVal = getStrPart(rgbColorStr, ',', 2).toInt();
      // TODO: save to EEPROM here
      mySerial.println("OK-");
    } else if (cmd.equals("GET_CONFIG-")) {
      // read from EEPROM
      mySerial.println("DATA-I:100;M:0;C:123,24,12");
      Serial.println("DATA-I:100;M:0;C:123,24,12");
    } else {
      mySerial.println("INVALID_COMMAND-"+incommingStr);
      Serial.println("SEND: INVALID_COMMAND-"+String(incommingStr));
    }
  }
}

void loop() {
  ESPUartListener();
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
