#include <Adafruit_NeoPixel.h>

/*
 * 
 * Mode list
 * 
 * Color configure:
    * Stable
    * Light spin
    * Night mode
    * Grow and blow
 * RGB Swipes
 * Rainbow Cycle
 * Smooth rainbow transition
 * Theater chase rainbow
 * Light show 1
 * Light show 2
 * 
 */
#define NEO_PIXEL_PIN 6 // Arduino pin number (most are valid)
#define NUM_PIXEL 29
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXEL, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

void setup() {
  Serial.begin(9600);
  strip.begin();
  strip.setBrightness(50);
  strip.show(); // Initialize all pixels to 'off'
}

void loop() {
//  lightshow01();

//  blackHoleTravel(5, 5, 20);
//  delay(1000);
  lightshow02();
  delay(2000);
}

void lightshow02() {
  uint8_t waits[] = {20, 30, 45, 55, 60};
  uint8_t wait = 70;
  for (int i=0; i<5; i++) {
    blackHoleTravelRight(4, 0, random(0, strip.numPixels()), wait - waits[i]);
    delay(1000);
  }

}

void lightshow01() {
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
  strip.setBrightness(50);
}


void blackHoleTravelRight(uint8_t lightLength, uint8_t startPos, uint8_t endPos, uint8_t wait) {
  if (lightLength < 1 || startPos < 0 || endPos > strip.numPixels()) {
    Serial.println(F("Input error blackHoleTravelRight"));
    return;
  }
  
  for (int i=startPos; i<=endPos+lightLength; i++) {
    Serial.println("i " + i);
    if (i <= endPos) {
      strip.setPixelColor(i, strip.Color(0, 255, 0));
    }
    
    if (i - lightLength >= 0) {
      strip.setPixelColor(i-lightLength, strip.Color(0, 0, 0));
    }
    strip.show();
    delay(wait);
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
  }

  delay(1400);
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
  }
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
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
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

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
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
