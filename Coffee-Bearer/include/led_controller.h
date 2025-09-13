#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// Enum para os tipos de animação de sinalização
enum AnimationType {
    ANIM_NONE,
    ANIM_BLINK,
    ANIM_ALTERNATE
};

class LEDController {
private:
    Adafruit_NeoPixel* strip;
    bool initialized;

    // --- Variáveis de Estado Contínuo ---
    uint32_t continuousColor; // Cor base (Verde OK, Vermelho Vazio, etc.)

    // --- Variáveis de Animação (para não bloquear) ---
    AnimationType currentAnimation;
    unsigned long animationStartTime;
    uint32_t animColor1;
    uint32_t animColor2;
    int animStep;

    // Cores
    const uint32_t COLOR_RED = Adafruit_NeoPixel::Color(255, 0, 0);
    const uint32_t COLOR_GREEN = Adafruit_NeoPixel::Color(0, 255, 0);
    const uint32_t COLOR_BLUE = Adafruit_NeoPixel::Color(0, 0, 255);
    const uint32_t COLOR_YELLOW = Adafruit_NeoPixel::Color(255, 255, 0);
    const uint32_t COLOR_OFF = Adafruit_NeoPixel::Color(0, 0, 0);

    void runAnimation();

public:
    LEDController();
    ~LEDController();

    // Inicialização
    bool begin();

    // --- MÉTODOS DE ESTADO CONTÍNUO ---
    void showStatusOK();
    void showStatusInitializing();
    void showStatusEmpty();
    void showStatusLow();
    void turnOff();

    // --- MÉTODOS DE SINALIZAÇÃO (NÃO-BLOQUEANTES) ---
    void signalServing();
    void signalNoCredits();
    void signalMasterKey();
    void signalUnknownUser();
    void signalError();

    // Loop de atualização (DEVE ser chamado a cada ciclo do loop principal)
    void update();
};