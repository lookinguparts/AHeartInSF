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

#define LED_TYPE WS2811
#define DATA_PIN 2
#define NUM_LEDS 58  // number of LEDs, including null pixel
#define COLOR_ORDER GBR
CRGB leds[NUM_LEDS + 1];
CRGB colors[NUM_LEDS];
#define BRIGHTNESS 255
const int analogInPin = A14;  //A Photo Transistor Sensor is attached, using a 10k Ohm resistor
int sensorValue = 0; // value read from the pot

uint8_t bloodHue = 96;        // Blood color [hue from 0-255]
uint8_t bloodSat = 255;       // Blood staturation [0-255]
int flowDirection = -1;       // Use either 1 or -1 to set flow direction
uint16_t cycleLength = 1500;  // Lover values = continuous flow, higher values = distinct pulses.
uint16_t pulseLength = 300;   // How long the pulse takes to fade out.  Higher value is longer.
uint16_t pulseOffset = 350;   // Delay before second pulse.  Higher value is more delay.
uint8_t baseBrightness = 30;  // Brightness of LEDs when not pulsing. Set to 0 for off.

uint32_t windowTransitionMillis = 500;       //Old val 200
uint32_t windowTransitionIntervalMin = 300;  //Old val 500
uint32_t windowTransitionIntervalMax = 2000;

#define WINDOW_COUNT 11
std::array<uint8_t, WINDOW_COUNT> windowLedCounts{
  { 3, 6, 2, 4, 8, 8, 4, 8, 2, 6, 6 }
};
std::array<uint8_t, WINDOW_COUNT + 1> windowLedOffsets;
std::array<uint8_t, WINDOW_COUNT + 1> windowBrightness{ 0 };
uint32_t nextTransitionTime = 0;
bool isWindowTransitioning = false;
uint8_t transitioningWindowIndex = 0;
bool transitioningWindowDirection = false;  // false: turn off, true: turn on

//---------------------------------------------------------------
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

  nextTransitionTime = millis() + random(windowTransitionIntervalMin, windowTransitionIntervalMax);
}

//---------------------------------------------------------------
void loop() {
// read the analog in value:
  sensorValue = analogRead(analogInPin);  
  uint32_t currentTime = millis();

  if (!isWindowTransitioning && currentTime > nextTransitionTime) {
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
    uint32_t delta = currentTime - nextTransitionTime;
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
      nextTransitionTime = currentTime + random(windowTransitionIntervalMin, windowTransitionIntervalMax);
    }
  }

  //heartBeat();  // Heart beat function
  if (sensorValue > 4) {
fill_solid(colors, NUM_LEDS, CRGB::Pink);
  }
   if (sensorValue < 4) {
fill_solid(colors, NUM_LEDS, CRGB::Red);
  }

  leds[0] = CRGB::White;  // null pixel

  for (int i = 0; i < WINDOW_COUNT; ++i) {
    for (int j = windowLedOffsets[i]; j < windowLedOffsets[i + 1]; ++j) {
      leds[j + 1] = colors[j].nscale8(windowBrightness[i]);
    }
  }

  FastLED.show();
}


void setWindowColor(int i, CRGB c) {
  for (int j = windowLedOffsets[i]; j < windowLedOffsets[i + 1]; ++j) {
    colors[j] = c;
  }
}

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
