// in src/beeps_and_blips.cpp

#include "beeps_and_bleeps.h"

FeedbackManager::FeedbackManager() :
    strip(nullptr),
    ledState(LED_STATIC),
    staticColor(0),
    currentAnimation(ANIM_NONE),
    animationStartTime(0),
    animColor1(0), animColor2(0),
    animBlinks(0), animDuration(0),
    buzzerState(BUZZER_IDLE),
    toneQueueIndex(0),
    nextToneTime(0) 
{
    memset(toneQueue, 0, sizeof(toneQueue));
}
FeedbackManager::~FeedbackManager() {
    if (strip) {
        delete strip;
    }
}

bool FeedbackManager::begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    strip = new Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
    if (!strip) return false;

    strip->begin();
    strip->setBrightness(128);
    strip->clear();
    strip->show();
    
    showStatusInitializing();
    return true;
}

void FeedbackManager::update() {
    updateLed();
    updateBuzzer();
}

// --- Continuous Status Methods ---
void FeedbackManager::showStatusReady() { ledState = LED_STATIC; staticColor = strip->Color(0, 150, 0); }
void FeedbackManager::showStatusBusy() { ledState = LED_STATIC; staticColor = strip->Color(255, 165, 0); }
void FeedbackManager::showStatusLow() { ledState = LED_STATIC; staticColor = strip->Color(0, 150, 200); }
void FeedbackManager::showStatusEmpty() { ledState = LED_STATIC; staticColor = strip->Color(200, 0, 0); }
void FeedbackManager::showStatusError() { ledState = LED_STATIC; staticColor = strip->Color(255, 0, 0); }
void FeedbackManager::showStatusInitializing() { ledState = LED_STATIC; staticColor = strip->Color(0, 0, 200); }
void FeedbackManager::turnOff() { ledState = LED_STATIC; staticColor = 0; }


// --- Event Signal Methods ---
void FeedbackManager::signalSuccess() {
    const int sequence[] = { TONE_SUCCESS_FREQ1, TONE_SUCCESS_DURATION, 50, TONE_SUCCESS_FREQ2, TONE_SUCCESS_DURATION, 0 };
    playToneSequence(sequence);
    
    ledState = LED_ANIMATING;
    currentAnimation = ANIM_BLINK;
    animationStartTime = millis();
    animColor1 = strip->Color(0, 255, 0);
    animBlinks = 2;
    animDuration = 800;
}

void FeedbackManager::signalError() {
    const int sequence[] = { TONE_ERROR_FREQ, TONE_ERROR_DURATION, 0 };
    playToneSequence(sequence);
    
    ledState = LED_ANIMATING;
    currentAnimation = ANIM_BLINK;
    animationStartTime = millis();
    animColor1 = strip->Color(255, 0, 0);
    animBlinks = 3;
    animDuration = 1000;
}

void FeedbackManager::signalServing() {
    const int sequence[] = { TONE_COFFEE_FREQ1, TONE_COFFEE_DURATION, 50, TONE_COFFEE_FREQ2, TONE_COFFEE_DURATION, 0 };
    playToneSequence(sequence);
    showStatusBusy();
}


void FeedbackManager::signalRefill() {
    const int sequence[] = { TONE_REFILL_FREQ1, 100, 50, TONE_REFILL_FREQ2, 100, 50, TONE_REFILL_FREQ3, 100, 0 };
    playToneSequence(sequence);
    
    ledState = LED_ANIMATING;
    currentAnimation = ANIM_ALTERNATE;
    animationStartTime = millis();
    animColor1 = strip->Color(255, 255, 0);
    animColor2 = strip->Color(0, 0, 255);
    animBlinks = 6;
    animDuration = 1200;
}

void FeedbackManager::signalMasterKey() { signalRefill(); }
void FeedbackManager::signalUnknownUser() { signalError(); }
void FeedbackManager::signalNoCredits() {
    const int sequence[] = { TONE_ERROR_FREQ, 100, 50, TONE_ERROR_FREQ, 200, 0 };
    playToneSequence(sequence);
    
    ledState = LED_ANIMATING;
    currentAnimation = ANIM_ALTERNATE;
    animationStartTime = millis();
    animColor1 = strip->Color(255, 0, 0);
    animColor2 = strip->Color(255, 165, 0);
    animBlinks = 4;
    animDuration = 1000;
}


// --- Private LED State Machine ---
void FeedbackManager::updateLed() {
    if (ledState == LED_STATIC) {
        if (strip->getPixelColor(0) != staticColor) {
            strip->setPixelColor(0, staticColor);
            strip->show();
        }
        return;
    }
    
    switch (currentAnimation) {
        case ANIM_BLINK:
            update_animBlink();
            break;
        case ANIM_ALTERNATE:
            update_animAlternate();
            break;
        case ANIM_NONE:
        default:
            ledState = LED_STATIC;
            break;
    }
}

void FeedbackManager::update_animBlink() {
    unsigned long elapsed = millis() - animationStartTime;
    if (elapsed > animDuration) {
        currentAnimation = ANIM_NONE;
        ledState = LED_STATIC;
        return;
    }
    int interval = animDuration / (animBlinks * 2);
    bool on = (elapsed / interval) % 2 == 0;
    strip->setPixelColor(0, on ? animColor1 : 0);
    strip->show();
}

void FeedbackManager::update_animAlternate() {
    unsigned long elapsed = millis() - animationStartTime;
    if (elapsed > animDuration) {
        currentAnimation = ANIM_NONE;
        ledState = LED_STATIC;
        return;
    }
    int interval = animDuration / animBlinks;
    bool useColor1 = (elapsed / interval) % 2 == 0;
    strip->setPixelColor(0, useColor1 ? animColor1 : animColor2);
    strip->show();
}

// --- Private Buzzer State Machine ---
void FeedbackManager::playToneSequence(const int sequence[]) {
    if (buzzerState != BUZZER_IDLE) return;
    
    int i = 0;
    while(i < 10 && sequence[i] != 0) {
        toneQueue[i] = sequence[i];
        i++;
    }
    toneQueue[i] = 0;
    toneQueueIndex = 0;
    nextToneTime = millis();
    buzzerState = BUZZER_PLAYING;
}

void FeedbackManager::updateBuzzer() {
    if (buzzerState == BUZZER_IDLE || millis() < nextToneTime) {
        return;
    }

    int freq = toneQueue[toneQueueIndex++];
    int duration = toneQueue[toneQueueIndex++];

    if (freq == 0) {
        buzzerState = BUZZER_IDLE;
        noTone(BUZZER_PIN);
        return;
    }
    
    if (freq > 0) {
        tone(BUZZER_PIN, freq, duration);
    }
    
    nextToneTime = millis() + duration;
}