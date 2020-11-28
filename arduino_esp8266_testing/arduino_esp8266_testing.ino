#include <SoftwareSerial.h>

SoftwareSerial mySerial(2,3); // RX, TX
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  mySerial.begin(9600);
  delay(1000);
  Serial.println("start");
}

void loop() {
  String incommingStr = "";
  boolean stringReady = false;

  while (mySerial.available()) {
    incommingStr = mySerial.readString();
    stringReady = true;
  }

  if (stringReady) {
    Serial.println("Received: " + incommingStr);
  }
  // put your main code here, to run repeatedly:

}
