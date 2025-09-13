#include "led_controller.h"

#define ANIMATION_TOTAL_DURATION 1200 // Duração total das animações em ms
#define BLINK_INTERVAL 200            // Intervalo de piscada (150ms on, 150ms off)

LEDController::LEDController() :
    strip(nullptr),
    initialized(false),
    continuousColor(COLOR_OFF),
    currentAnimation(ANIM_NONE),
    animationStartTime(0),
    animStep(0) {
}

LEDController::~LEDController() {
    if (strip) {
        delete strip;
    }
}

bool LEDController::begin() {
    if (initialized) return true;
    strip = new Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
    if (!strip) return false;
    strip->begin();
    strip->setBrightness(128);
    strip->clear();
    strip->show();
    initialized = true;
    return true;
}

// --- MÉTODOS DE ESTADO CONTÍNUO ---
// Estes métodos cancelam qualquer animação e definem a cor de fundo.
void LEDController::showStatusOK() {
    currentAnimation = ANIM_NONE;
    continuousColor = COLOR_GREEN;
}

void LEDController::showStatusEmpty() {
    currentAnimation = ANIM_NONE;
    continuousColor = COLOR_RED;
}

void LEDController::showStatusLow() {
    currentAnimation = ANIM_NONE;
    continuousColor = COLOR_YELLOW;
}

void LEDController::turnOff() {
    currentAnimation = ANIM_NONE;
    continuousColor = COLOR_OFF;
}

// --- MÉTODOS DE SINALIZAÇÃO (NÃO-BLOQUEANTES) ---
// Estes métodos apenas preparam a animação para ser executada no `update()`.
void LEDController::signalServing() {
    currentAnimation = ANIM_BLINK;
    animColor1 = COLOR_GREEN;
    animationStartTime = millis();
    animStep = 0;
}

void LEDController::signalNoCredits() {
    currentAnimation = ANIM_BLINK;
    animColor1 = COLOR_RED;
    animationStartTime = millis();
    animStep = 0;
}

void LEDController::signalUnknownUser() {
    currentAnimation = ANIM_ALTERNATE;
    animColor1 = COLOR_YELLOW;
    animColor2 = COLOR_RED;
    animationStartTime = millis();
    animStep = 0;
}

// --- LOOP PRINCIPAL ---
void LEDController::update() {
    if (!initialized) return;

    if (currentAnimation != ANIM_NONE) {
        runAnimation();
    } else {
        // Se nenhuma animação estiver ativa, apenas exibe a cor de estado.
        strip->setPixelColor(0, continuousColor);
        strip->show();
    }
}

void LEDController::runAnimation() {
    unsigned long elapsedTime = millis() - animationStartTime;

    // Se a animação terminou, volte ao estado contínuo
    if (elapsedTime >= ANIMATION_TOTAL_DURATION) {
        currentAnimation = ANIM_NONE;
        return;
    }

    switch (currentAnimation) {
        case ANIM_BLINK: {
            // Lógica para 3 piscadas em ~1200ms
            int currentStep = elapsedTime / BLINK_INTERVAL;
            if (currentStep < 6 && currentStep % 2 == 0) { // Pisca nos intervalos 0, 2, 4
                strip->setPixelColor(0, animColor1);
            } else {
                strip->clear();
            }
            break;
        }
        case ANIM_ALTERNATE: {
            // Lógica para alternar cores
            int currentStep = elapsedTime / BLINK_INTERVAL;
            if (currentStep % 2 == 0) {
                strip->setPixelColor(0, animColor1); // Amarelo
            } else {
                strip->setPixelColor(0, animColor2); // Vermelho
            }
            break;
        }
        case ANIM_NONE:
            break;
    }
    strip->show();
}