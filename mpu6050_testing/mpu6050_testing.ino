// I2C device class (I2Cdev) demonstration Arduino sketch for MPU6050 class
// 10/7/2011 by Jeff Rowberg <jeff@rowberg.net>
// Updates should (hopefully) always be available at https://github.com/jrowberg/i2cdevlib
//
// Changelog:
//      2013-05-08 - added multiple output formats
//                 - added seamless Fastwire support
//      2011-10-07 - initial release

/* ============================================
I2Cdev device library code is placed under the MIT license
Copyright (c) 2011 Jeff Rowberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
===============================================
*/

// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"
#include "MPU6050.h"

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 accelgyro;
//MPU6050 accelgyro(0x69); // <-- use for AD0 high

int16_t ax, ay, az;
int16_t gx, gy, gz;



// uncomment "OUTPUT_READABLE_ACCELGYRO" if you want to see a tab-separated
// list of the accel X/Y/Z and then gyro X/Y/Z values in decimal. Easy to read,
// not so easy to parse, and slow(er) over UART.
#define OUTPUT_READABLE_ACCELGYRO

// uncomment "OUTPUT_BINARY_ACCELGYRO" to send all 6 axes of data as 16-bit
// binary, one right after the other. This is very fast (as fast as possible
// without compression or data loss), and easy to parse, but impossible to read
// for a human.
//#define OUTPUT_BINARY_ACCELGYRO


#define LED_PIN 13
bool blinkState = false;


float axMean, ayMean, azMean, totalAccel;

unsigned long accelPrecisionCount;
unsigned long startShake, endShake, prevShakeStamp;
float startAccAccel, endAccAccel, prevAccAccel;

float accShakeArea;

typedef enum {
  IDLE_POS,
  FIRST_SHAKE,
  SHAKING,
  END_SHAKING
} shake_state;

shake_state shakeState;

void setup() {
    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    // initialize serial communication
    // (38400 chosen because it works as well at 8MHz as it does at 16MHz, but
    // it's really up to you depending on your project)
    Serial.begin(38400);

    // initialize device
    Serial.println("Initializing I2C devices...");
    accelgyro.initialize();

    // verify connection
    Serial.println("Testing device connections...");
    Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

    accelgyro.setXAccelOffset(2056);
    accelgyro.setYAccelOffset(-552);
    accelgyro.setZAccelOffset(1482); // stable Z Accel: 16384(earth gravity)
    Serial.println("totalAccel,span,area");

    shakeState = IDLE_POS;
    accelPrecisionCount = 0;
    startAccAccel = 0;
    endAccAccel = 0;
}


void loop() {
    axMean = 0;
    ayMean = 0;
    azMean = 0;
    for (int i=0; i<10; i++) {
      accelgyro.getAcceleration(&ax, &ay, &az);
      az -= 16384;
      axMean += (float) ax;
      ayMean += (float) ay;
      azMean += (float) az;
      delay(1);
    }
    axMean /= 10;
    ayMean /= 10;
    azMean /= 10;
    #define TOTAL_ACCEL_OFFSET 470
    totalAccel = sqrt(axMean * axMean + ayMean * ayMean + azMean * azMean) - TOTAL_ACCEL_OFFSET;

    //shaking period
    // shake > FIRST_SHAKE_THRESHOLD
    // still a shake if shake value > END_SHAKE_THRESHOLD
    #define FIRST_SHAKE_THRESHOLD 2000
    #define END_SHAKE_THRESHOLD 500

    if (totalAccel > FIRST_SHAKE_THRESHOLD && shakeState == IDLE_POS) {
      shakeState = FIRST_SHAKE;
    }

    if (totalAccel < END_SHAKE_THRESHOLD && shakeState != IDLE_POS){
      shakeState = END_SHAKING;
    }

//    Serial.print(totalAccel);
//    Serial.print(",");
    if (shakeState == FIRST_SHAKE) {
      startShake = millis();
      accShakeArea = totalAccel;
      prevAccAccel = totalAccel;
      prevShakeStamp = startShake;
      shakeState = SHAKING;
//      Serial.println(",");
    } else if (shakeState == SHAKING) {
      accShakeArea += (totalAccel + prevAccAccel)/2. * ((float) millis() - (float) prevShakeStamp);
      prevShakeStamp = millis();
      prevAccAccel = totalAccel;
//      Serial.println(",");
    } else if (shakeState == END_SHAKING) {
      endShake = millis();
      // check if that was an actual shake or not.
      Serial.print("startShake, endShake  "); Serial.print(startShake); Serial.print(","); Serial.print(endShake); Serial.print("==>");
      Serial.println(endShake - startShake);
      Serial.print("under the curve: "); Serial.println(accShakeArea);
//      Serial.print(endShake - startShake); Serial.print(",");Serial.println(accShakeArea);
      accShakeArea = 0.;
      shakeState = IDLE_POS;
    } else {
//      Serial.println(",");
    }
//    Serial.print(millis());Serial.print(",");
//    Serial.println(totalAccel);
}
