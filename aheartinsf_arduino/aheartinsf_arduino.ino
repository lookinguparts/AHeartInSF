//**********************************************************************
// Windows in to Art, an interactive LED heart sculpture in
// San Francisco by Josh Zubkoff, Srikanth Guttikonda, Dede Lucia
// -Software Contributers
//  LED patterns by Hike Danakian
//  An LED pattern base code from Heart Pulse, by Marc Miller, Jan 2016
//  An LED pattern base code from Color palette, a FastLED library example
// -Fabrication Contributers
//  Kyle Matlock
//  Sarah King
//**********************************************************************
#include <array>
#include "FastLED.h"
#include <runningAvg.h>   // The include of the .h file.

#define LED_TYPE WS2811
#define DATA_PIN 2
#define NUM_LEDS 58  // number of LEDs, including null pixel
#define COLOR_ORDER RBG
#define BRIGHTNESS 255

CRGB leds[NUM_LEDS+1];
CRGB colors[NUM_LEDS];

//Window color: Yellow
uint8_t windowHue = 42;
uint8_t windowSat = 255;
uint8_t windowVal = 255;

//A Photo Transistor Sensor is attached, using a 10k Ohm resistor
const int analogInPin = A14;  
int sensorValue = 0; // value read from the pot
int ave = 0; //average sensor value
  runningAvg  smoother(10);   // Create a running average of 5 value smoother. (global variable)

//Pattern trasitions
uint32_t patternTransitionMillis = 1000;
uint32_t patternTransitionInterval = 1 * 60000;

//Blood pattern related
uint8_t bloodHue = 96;        // Blood color [hue from 0-255]
uint8_t bloodSat = 255;       // Blood staturation [0-255]
int flowDirection = -1;       // Use either 1 or -1 to set flow direction
uint16_t cycleLength = 1500;  // Lover values = continuous flow, higher values = distinct pulses.
uint16_t pulseLength = 300;   // How long the pulse takes to fade out.  Higher value is longer.
uint16_t pulseOffset = 350;   // Delay before second pulse.  Higher value is more delay.
uint8_t baseBrightness = 30;  // Brightness of LEDs when not pulsing. Set to 0 for off.

//Window lights pattern
uint32_t windowTransitionMillis = 500;       //Old val 200
uint32_t windowTransitionIntervalMin = 300;  //Old val 500
uint32_t windowTransitionIntervalMax = 2000;
uint32_t rainbowFadeSpeed = 50; //To control the speed							   

#define WINDOW_COUNT 11
std::array<uint8_t, WINDOW_COUNT> windowLedCounts{
  {3, 6, 2, 4, 8, 8, 4, 8, 2, 6, 6}
};
std::array<uint8_t, WINDOW_COUNT+1> windowLedOffsets;
std::array<uint8_t, WINDOW_COUNT> windowBrightness{0};
uint32_t nextWindowTransitionTime = 0;
bool isWindowTransitioning = false;
uint8_t transitioningWindowIndex = 0;
bool transitioningWindowDirection = false; // false: turn off, true: turn on


#define PATTERN_COUNT 3
uint8_t currentPattern = 0; // 0: window flash, 1: rainbow, 2: heartbeat
uint32_t nextPatternTransitionTime = 0;
uint8_t transitionMode = 0; // 0: none, 1: fade out, 2: fade in
uint8_t transitionToPattern = 0;
uint8_t transitionFade = 0;
uint32_t transitionStartTime = 0;  
//--------------------------------------------------------------------------------------------------

void setup() {
  int ledCount = 0;
  for (int i = 0; i < WINDOW_COUNT; ++i) {
    windowLedOffsets[i] = ledCount;
    ledCount += windowLedCounts[i];
  }
  windowLedOffsets[WINDOW_COUNT] = ledCount;

  Serial.begin(115200);
  delay(3000);  // 3 second delay
  //FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}

//---------------------------------------------------------------
void loop() {
// read the analog in value:
  sensorValue = analogRead(analogInPin);
  ave = smoother.addData(sensorValue);// Pop this number into our smoother. (Running average object.) Out pops the average.]  
 
 switch (currentPattern) {
  case 0:
    windowFlash();
    break;
  case 1:
    rainbowFade();
    break;
  case 2:
    heartBeat();
    break;
  }

  transition();

  leds[0] = CRGB::White; // null pixel

  for (int i = 0; i < WINDOW_COUNT; ++i) {
    for (int j = windowLedOffsets[i]; j < windowLedOffsets[i + 1]; ++j) {
      leds[j + 1] = colors[j];

      if (transitionMode != 0) {
        leds[j + 1] = leds[j + 1].fadeToBlackBy(transitionFade);
      }
    }
  }

  FastLED.show();
}

void switchPattern(uint8_t pattern) {
  switch (transitionMode) {
  case 0: // no current transition
    transitionMode = 1;
    transitionFade = 0;
    transitionToPattern = pattern;
    transitionStartTime = millis();
    break;
  case 1: // currently fading out
    transitionToPattern = pattern;
    break;
  case 2: // currently fading in
    transitionStartTime = millis() - (patternTransitionMillis - (millis() - transitionStartTime));
    transitionFade = 255 - transitionFade;
    transitionMode = 1;
    transitionToPattern = pattern;
    break;
  }
}

void transition() {
  uint32_t currentTime = millis();

  if (!transitionMode) {
    if (nextPatternTransitionTime == 0) {
      nextPatternTransitionTime = currentTime + patternTransitionInterval;
    }

    if (currentTime > nextPatternTransitionTime) {
      uint8_t nextPattern;
      do {
        nextPattern = random(PATTERN_COUNT);
      } while (nextPattern == currentPattern);
      switchPattern(nextPattern);
      nextPatternTransitionTime = currentTime + patternTransitionInterval;
    }

    return;
  }

  uint32_t delta = currentTime - transitionStartTime;
  uint8_t phase = delta < patternTransitionMillis ? delta * 255 / patternTransitionMillis : 255;
  transitionFade = transitionMode == 1 ? phase : 255 - phase; // fade out vs in

  if (phase == 255) {
    transitionMode = (transitionMode + 1) % 3;

    if (transitionMode == 2) {
      currentPattern = transitionToPattern;
      transitionStartTime = millis();
    }
  }
}

void setWindowColor(int i, CRGB c) {
  for (int j = windowLedOffsets[i]; j < windowLedOffsets[i + 1]; ++j) {
    colors[j] = c;
  }
}

void windowFlash() {
  uint32_t currentTime = millis();

  if (nextWindowTransitionTime == 0) {
    nextWindowTransitionTime = currentTime + random(windowTransitionIntervalMin, windowTransitionIntervalMax);
  }

  if (!isWindowTransitioning && currentTime > nextWindowTransitionTime) {
    /*
    do {
      transitioningWindowIndex = random(WINDOW_COUNT);
    } while (windowBrightness[transitioningWindowIndex] != 0);
    */

    transitioningWindowIndex = random(WINDOW_COUNT);
    transitioningWindowDirection = windowBrightness[transitioningWindowIndex] == 0;

    //printf("Turning %s window %d\n", (transitioningWindowDirection ? "on": "off"), transitioningWindowIndex);
    isWindowTransitioning = true;
  }

  if (isWindowTransitioning) {
    uint32_t delta = currentTime - nextWindowTransitionTime;
    uint8_t phase = delta < windowTransitionMillis ? delta * 255 / windowTransitionMillis : 255;

    // linear
    //windowBrightness[transitioningWindowIndex] = phase;

    // cubic
    windowBrightness[transitioningWindowIndex] = ease8InOutCubic(phase / 2) * 2;
    if (windowBrightness[transitioningWindowIndex] == 254) {
      windowBrightness[transitioningWindowIndex] = 255;
    }

    if (!transitioningWindowDirection) {
      windowBrightness[transitioningWindowIndex] = 255 - windowBrightness[transitioningWindowIndex];
    }

    //printf("Setting window %d brightness to %d (%d)\n", transitioningWindowIndex, windowBrightness[transitioningWindowIndex], phase);

    if (phase == 255) {
      isWindowTransitioning = false;
      nextWindowTransitionTime = currentTime + random(windowTransitionIntervalMin, windowTransitionIntervalMax);
    }
  }

  //Window color is applied here
  //Old code: fill_solid(colors, NUM_LEDS, CRGB::Yellow);
  for (int i = 0; i < WINDOW_COUNT; ++i) {
    for (int j = windowLedOffsets[i]; j < windowLedOffsets[i + 1]; ++j) {
      CRGB c;
      colors[j] = c.setHSV(windowHue, windowSat, windowVal).nscale8(windowBrightness[i]);
    }
  }				 
}

void rainbowFade() {
  static uint32_t startIndex = 0; // last color index scaled by 1000
  static uint32_t lastTime = millis();

  uint8_t colorIndex = startIndex / 1000;
  for (int i = 0; i < NUM_LEDS; ++i) {
    colors[i] = ColorFromPalette(RainbowColors_p, colorIndex, 255, LINEARBLEND);
    ++colorIndex;
  }

  uint32_t currentTime = millis();
  uint32_t delta = currentTime - lastTime;
  lastTime = currentTime;
  startIndex += rainbowFadeSpeed * delta;
}

//heartBeat();  // Heart beat function
//Using of sensor data to change pattern
  //if (ave > 4) {
//fill_solid(colors, NUM_LEDS, CRGB::Red);
  //}
  // if (ave < 4) {
//fill_solid(colors, NUM_LEDS, CRGB::HotPink);
//  }
//  leds[0] = CRGB::White;  // null pixel

//===============================================================
// Heart Beat Functions
//   The base for all this goodness came from Mark Kriegsman and
//   was initially coded up by Chris Thodey.  I updated it to use
//   HSV and provided all the variables to play with up above.
//   -Marc

void heartBeat() {
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t bloodVal = sumPulse((5 / NUM_LEDS / 2) + (NUM_LEDS / 2) * i * flowDirection);
    colors[i] = CHSV(bloodHue, bloodSat, bloodVal);
  }
}

int sumPulse(int time_shift) {
  //time_shift = 0;  //Uncomment to heart beat/pulse all LEDs together
  int pulse1 = pulseWave8(millis() + time_shift, cycleLength, pulseLength);
  int pulse2 = pulseWave8(millis() + time_shift + pulseOffset, cycleLength, pulseLength);
  return qadd8(pulse1, pulse2);  // Add pulses together without overflow
}

uint8_t pulseWave8(uint32_t ms, uint16_t cycleLength, uint16_t pulseLength) {
  uint16_t T = ms % cycleLength;
  if (T > pulseLength) return baseBrightness;
  uint16_t halfPulse = pulseLength / 2;
  if (T <= halfPulse) {
    return (T * 255) / halfPulse;  //first half = going up
  } else {
    return ((pulseLength - T) * 255) / halfPulse;  //second half = going down
  }
}
//End_heart_beat_functions
//---------------------------------------------------------------
