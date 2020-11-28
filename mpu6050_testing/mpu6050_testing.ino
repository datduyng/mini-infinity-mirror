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
    Wire.begin();

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


void printXYZ(float ax, float ay, float az) {
  Serial.print("ax, ay, az: "); Serial.print(ax); Serial.print(",");Serial.print(ay); Serial.print(",");Serial.print(az); 
  Serial.println();
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
    printXYZ(axMean, ayMean, azMean);
    delay(100);
}
