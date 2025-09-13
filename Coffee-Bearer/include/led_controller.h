/*
==================================================
CONTROLADOR DE LED NEOPIXEL
Gerencia indicações visuais do sistema
==================================================
*/

#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

enum LEDState {
    LED_OFF,
    LED_READY,
    LED_SERVING,
    LED_ERROR,
    LED_EMPTY,
    LED_CONNECTING,
    LED_INITIALIZING,
    LED_SUCCESS
};

enum LEDPattern {
    PATTERN_SOLID,
    PATTERN_PULSE,
    PATTERN_CHASE,
    PATTERN_RAINBOW,
    PATTERN_FADE
};

class LEDController {
private:
    Adafruit_NeoPixel* strip;
    LEDState currentState;
    LEDPattern currentPattern;
    unsigned long lastUpdate;
    int animationStep;
    uint32_t primaryColor;
    uint32_t secondaryColor;
    bool initialized;
    
    // Métodos internos para padrões
    void updateSolid();
    void updatePulse();
    void updateChase();
    void updateRainbow();
    void updateFade();
    
    // Utilitários de cor
    uint32_t wheel(byte wheelPos);
    uint32_t gamma32(uint32_t color);
    uint32_t blend(uint32_t color1, uint32_t color2, uint8_t blend);
    uint8_t sine8(uint8_t x);
    
public:
    LEDController();
    ~LEDController();
    
    // Inicialização
    bool begin();
    void end();
    
    // Controle de estado
    void setOff();
    void setReady();
    void setServing();
    void setError();
    void setEmpty();
    void setConnecting();
    void setInitializing();
    void setSuccess();
    
    // Controle manual
    void setCustom(uint32_t color, LEDPattern pattern = PATTERN_SOLID);
    void setCustomDual(uint32_t color1, uint32_t color2, LEDPattern pattern);
    
    // Padrões especiais
    void startRainbow();
    void showProgress(uint8_t percentage);
    void flashColor(uint32_t color, int count = 3);
    
    // Controle de brilho
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness();
    
    // Atualização (deve ser chamado no loop principal)
    void update();
    
    // Status
    LEDState getState() { return currentState; }
    LEDPattern getPattern() { return currentPattern; }
    bool isInitialized() { return initialized; }
    
    // Utilitários públicos de cor
    static uint32_t colorRGB(uint8_t r, uint8_t g, uint8_t b);
    static uint32_t colorHSV(uint16_t hue, uint8_t sat, uint8_t val);
    static void getRGB(uint32_t color, uint8_t* r, uint8_t* g, uint8_t* b);
};