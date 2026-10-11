#include <DaisyDuino.h>
using namespace daisysp;
DaisyHardware hw;

// Native Daisy Objects
Svf          myFilter1; // Dual SVF filters cascaded for steep 24dB/octave slope
Svf          myFilter2;
PitchShifter myArp;
Overdrive    myDrive;

// Global Volumes & Pre-Amp
float inputBoost     = 1.2f; 
float masterVolume   = 1.0f;

// 1-Second Buffer
#define DELAY_MAX_SAMPLES 48000
float delayBuffer[DELAY_MAX_SAMPLES];
int   writePtr = 0;

// Global Variables
float targetDistortion = 0.0f;
float filterCutoff     = 15000.0f;
float delayFeedback    = 0.0f;

// Delay variables
float delayTargetDelay  = 500.0f;
float delayCurrentDelay = 500.0f; 

// Strum Detection
bool  strumTriggered = false;

// Stutter Control
bool  stutterActive = false;
int   stutterPeriod = 2000; 
int   stutterCounter = 0;
bool  stutterGate    = true;

// Arpeggiator Control (7 Steps for 2 Full Octaves)
int   arpState = 0; 
float arpSequence[7] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
int   arpStep = 0;
int   arpTimer = 0;
int   arpSpeedSamples = 12000; // ~250ms per note

// De-clicking Ramp Counter (4ms window @ 48kHz = 192 samples)
int   declickRamp = 0;
const int DECLICK_SAMPLES = 192;

// Button Tracking
bool lastButtonState = HIGH;

// -- Real-time audio proces chain --

void AudioCallback(float **in, float **out, size_t size) {
  
  // Safe block-level parameter updates
  myFilter1.SetFreq(filterCutoff);
  myFilter2.SetFreq(filterCutoff);
  myDrive.SetDrive(fclamp(targetDistortion * 0.85f, 0.0f, 0.85f));

  for (size_t i = 0; i < size; i++) {
    float signal = in[0][i] * inputBoost; 
    float cleanSignal = signal;
    float absSignal = fabsf(cleanSignal);

    // 1. Distortion mixing
    if (targetDistortion > 0.01f) {
      float driveInput = SoftLimit(signal);
      float wetDrive = myDrive.Process(driveInput);
      
      wetDrive = SoftLimit(wetDrive * 0.45f);
      signal = (cleanSignal * (1.0f - (targetDistortion * 0.4f))) + (wetDrive * targetDistortion);
    }

    signal = SoftLimit(signal);

    // 2. LPF
    myFilter1.Process(signal);
    signal = myFilter1.Low(); 
    myFilter2.Process(signal);
    signal = myFilter2.Low(); 

    // DSP NaN/Inf Protection
    if (std::isnan(signal) || std::isinf(signal)) {
      signal = 0.0f;
      myFilter1.Init(48000.0f); 
      myFilter2.Init(48000.0f); 
    }

    // 3. Arp w/ 4ms attack
    if (arpState > 0) {
      bool stepChanged = false;

      // Strum Onset Detector
      if (absSignal > 0.04f && !strumTriggered) {
        strumTriggered = true;
        arpTimer = 0;
        arpStep = (arpStep + 1) % 7;
        myArp.SetTransposition(arpSequence[arpStep]);
        stepChanged = true;
      } else if (absSignal < 0.02f) {
        strumTriggered = false; 
      }

      // Timer Cycle
      arpTimer++;
      if (arpTimer >= arpSpeedSamples) {
        arpTimer = 0;
        arpStep = (arpStep + 1) % 7;
        myArp.SetTransposition(arpSequence[arpStep]);
        stepChanged = true;
      }

      // Trigger de-clicking ramp when step shifts
      if (stepChanged) {
        declickRamp = DECLICK_SAMPLES;
      }

      float wetArp = myArp.Process(signal);

      // Apply micro fade-in during pitch shifts to eradicate pops
      if (declickRamp > 0) {
        declickRamp--;
        float fade = 1.0f - ((float)declickRamp / (float)DECLICK_SAMPLES);
        wetArp *= fade;
      }

      signal = (signal * 0.30f) + (wetArp * 0.70f); 
    }

    // 4. Delay & Stutter
    delayCurrentDelay += (delayTargetDelay - delayCurrentDelay) * 0.001f; 

    int readPtr = writePtr - (int)delayCurrentDelay;
    if (readPtr < 0) readPtr += DELAY_MAX_SAMPLES;
    float delayOutput = delayBuffer[readPtr];

    delayBuffer[writePtr] = SoftLimit(signal + (delayOutput * delayFeedback));
    writePtr = (writePtr + 1) % DELAY_MAX_SAMPLES;

    signal = signal + (delayOutput * 0.5f);

    if (stutterActive) {
      stutterCounter++;
      if (stutterCounter >= stutterPeriod) {
        stutterCounter = 0;
        stutterGate = !stutterGate; 
      }
      if (!stutterGate) {
        signal *= 0.1f; 
      }
    }

    // 5. Volume
    float finalOutput = SoftLimit(signal * masterVolume);

    out[0][i] = finalOutput; 
    out[1][i] = finalOutput; 
  }
}

// -- Initialization --

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);

  analogReadResolution(12);

  myFilter1.Init(48000.0f);
  myFilter1.SetRes(0.20f); 
  myFilter2.Init(48000.0f);
  myFilter2.SetRes(0.20f); 

  myArp.Init(48000.0f);
  myDrive.Init();

  for (int i = 0; i < DELAY_MAX_SAMPLES; i++) {
    delayBuffer[i] = 0.0f;
  }

  pinMode(D19, INPUT_PULLUP);

  DAISY.begin(AudioCallback);
}


// -- Hardware Controls --

void loop() {
  // Knob 1: Distortion
  float pot1 = analogRead(A0) / 4095.0f;
  targetDistortion = powf(pot1, 1.3f); 

  // Knob 2: Filter
  float pot2 = analogRead(A1) / 4095.0f;
  float invertedPot2 = 1.0f - pot2;
  
  // Cubic curve gives deep control in low registers down to 40Hz
  filterCutoff = (invertedPot2 * invertedPot2 * invertedPot2 * 14960.0f) + 40.0f; 

  // Knob 3: Delay & Stutter
  float pot3 = analogRead(A2) / 4095.0f;
  if (pot3 <= 0.8f) {
    float normalZone = pot3 / 0.8f; 
    delayTargetDelay = (normalZone * (DELAY_MAX_SAMPLES - 1000)) + 500; 
    delayFeedback    = normalZone * 0.70f; 
    stutterActive    = false;
  } else {
    delayTargetDelay = DELAY_MAX_SAMPLES - 500;
    delayFeedback    = 0.70f;
    stutterActive    = true;
    float stutterZone = (pot3 - 0.8f) * 5.0f; 
    stutterPeriod = 4000 - (int)(stutterZone * 3500); 
  }

  // Knob 4: Volume
  masterVolume = (analogRead(A3) / 4095.0f) * 2.0f;

  // Arp Button
  bool currentButtonState = digitalRead(D19);
  
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    arpState = (arpState + 1) % 4; 
    arpStep = 0;                  

    if (arpState == 0) {
      // Arp Bypass
    } else if (arpState == 1) {
      // Major 7-Step 2-Octave Sweep
      arpSequence[0] = 0.0f;   
      arpSequence[1] = 4.0f;   
      arpSequence[2] = 7.0f;   
      arpSequence[3] = 12.0f;  
      arpSequence[4] = 16.0f;  
      arpSequence[5] = 19.0f;  
      arpSequence[6] = 24.0f;  
    } else if (arpState == 2) {
      // Minor 7-Step 2-Octave Sweep
      arpSequence[0] = 0.0f;   
      arpSequence[1] = 3.0f;   
      arpSequence[2] = 7.0f;   
      arpSequence[3] = 12.0f;  
      arpSequence[4] = 15.0f;  
      arpSequence[5] = 19.0f;  
      arpSequence[6] = 24.0f;  
    } else if (arpState == 3) {
      // Maj7 7-Step 2-Octave Sweep
      arpSequence[0] = 0.0f;   
      arpSequence[1] = 4.0f;   
      arpSequence[2] = 7.0f;   
      arpSequence[3] = 11.0f;  
      arpSequence[4] = 12.0f;  
      arpSequence[5] = 16.0f;  
      arpSequence[6] = 24.0f;  
    }
    delay(50); 
  }
  lastButtonState = currentButtonState;
}