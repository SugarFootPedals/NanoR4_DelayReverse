/****************************************************************************************************
  Digital Delay / Reverse Speech DSP Pedal for Guitar, Voice, Synths, etc.
  MODIFIED FOR ARDUINO NANO R4, using the onboard DAC for high-quality audio output.
  - Fastest sample rate possible for Nano R4 using Ticker (~48kHz).
  - Audio output on DAC0 (A0) at 12 bits.
****************************************************************************************************/

#include <Ticker.h>

// Pin assignments
#define AUDIO_IN        A1   // Buffered analog input
#define DELAY_TIME_POT  A2   // Delay time control
#define WET_DRY_POT     A3   // Wet/Dry mix control
#define REPEATS_POT     A6   // Echo repeats
#define SHIMMER_POT     A7   // Shimmer level
#define MODE_BUTTON     10   // Momentary switch for delay/reverse
#define DAC_OUT         A0   // DAC output (true 12-bit)

const unsigned int MAX_DELAY_BUF = 48000; // ~1 second at 48kHz, adjust as needed for RAM
volatile unsigned int delayTime = 2000;   // in samples, updated by pot
volatile bool reverseMode = false;
volatile byte repeats = 2;
volatile float wetMix = 0.5;
volatile float shimmerLevel = 0.0;

// Audio buffers
uint16_t delayBuffer[MAX_DELAY_BUF];
unsigned int writeIndex = 0;

// Helper variables
unsigned long lastButtonPress = 0;
bool lastButtonState = false;

// Ticker for audio ISR
Ticker audioTicker;

// Audio ISR called by Ticker at 48kHz
void audioISR() {
  // Read input (0-4095 for 12-bit ADC)
  uint16_t inputSample = analogRead(AUDIO_IN);

  // Write incoming sample to delay buffer
  delayBuffer[writeIndex] = inputSample;

  // Calculate read index for delay/reverse
  unsigned int readIndex;
  if (reverseMode) {
    if (writeIndex >= delayTime)
      readIndex = writeIndex - delayTime;
    else
      readIndex = MAX_DELAY_BUF + writeIndex - delayTime;
    readIndex %= MAX_DELAY_BUF;
  } else {
    readIndex = (writeIndex + MAX_DELAY_BUF - delayTime) % MAX_DELAY_BUF;
  }

  // Get delayed sample and apply repeats
  uint32_t delayedSample = delayBuffer[readIndex];
  for (byte r = 1; r <= repeats; r++) {
    unsigned int echoIndex = (readIndex + r * delayTime) % MAX_DELAY_BUF;
    delayedSample += delayBuffer[echoIndex] * 0.4; // attenuate each repeat
  }

  // Apply shimmer (simple high-frequency boost)
  int32_t shimmerSample = delayedSample + (delayedSample * shimmerLevel * ((delayedSample & 0xF) > 8 ? 1 : -1));

  // Wet/dry mix
  int32_t outputSample = (inputSample * (1.0 - wetMix)) + (shimmerSample * wetMix);

  // Output to DAC pin, constrain to 0-4095
  analogWrite(DAC_OUT, constrain(outputSample, 0, 4095));

  // Advance buffer index
  writeIndex = (writeIndex + 1) % MAX_DELAY_BUF;
}

void setup() {
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  pinMode(DAC_OUT, OUTPUT);

  analogReadResolution(12);    // 12 bits for ADC
  analogWriteResolution(12);   // 12 bits for DAC

  // Start Ticker ISR for audio at 48kHz (every 21us)
  audioTicker.attach_us(21, audioISR);
}

void loop() {
  // Handle delay/reverse toggle (debounced)
  bool buttonState = !digitalRead(MODE_BUTTON);
  if (buttonState && !lastButtonState && millis() - lastButtonPress > 150) {
    reverseMode = !reverseMode;
    lastButtonPress = millis();
  }
  lastButtonState = buttonState;

  // Read pots and update control variables
  unsigned int rawDelay = analogRead(DELAY_TIME_POT); // 0-4095
  delayTime = map(rawDelay, 0, 4095, 1, MAX_DELAY_BUF-1);

  unsigned int rawMix = analogRead(WET_DRY_POT);
  wetMix = rawMix / 4095.0; // 0.0 (dry) to 1.0 (wet)

  unsigned int rawRepeats = analogRead(REPEATS_POT);
  repeats = map(rawRepeats, 0, 4095, 0, 5);

  unsigned int rawShimmer = analogRead(SHIMMER_POT);
  shimmerLevel = rawShimmer / 4095.0; // 0.0 (none) to 1.0 (max)
}