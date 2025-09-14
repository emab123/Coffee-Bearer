// in include/beeps_and_blips.h

#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

class FeedbackManager {
public:
    FeedbackManager();
    ~FeedbackManager();

    // --- Core Methods ---
    bool begin();
    void update(); // Must be called in the main loop

    // --- Continuous Status Indicators ---
    void showStatusReady();
    void showStatusBusy();
    void showStatusLow();
    void showStatusEmpty();
    void showStatusError();
    void showStatusInitializing();
    void turnOff();

    // --- Event Signals (Non-Blocking) ---
    void signalSuccess();
    void signalError();
    void signalServing();
    void signalRefill();
    void signalMasterKey();
    void signalUnknownUser();
    void signalNoCredits();

private:
// --- LED Members ---
    Adafruit_NeoPixel* strip;
    enum LedState { LED_STATIC, LED_ANIMATING };
    LedState ledState;
    uint32_t staticColor;
    
    // LED Animation State Machine
    enum AnimationType { ANIM_NONE, ANIM_BLINK, ANIM_ALTERNATE };
    AnimationType currentAnimation;
    unsigned long animationStartTime;
    uint32_t animColor1, animColor2;
    int animBlinks, animDuration;

    void updateLed();
    void update_animBlink();
    void update_animAlternate();

    // --- Buzzer Members ---
    enum BuzzerState { BUZZER_IDLE, BUZZER_PLAYING };
    BuzzerState buzzerState;
    int toneQueue[10]; // Increased size for more complex sounds
    int toneQueueIndex;
    unsigned long nextToneTime;
    
    void playToneSequence(const int sequence[]);
    void updateBuzzer();
};