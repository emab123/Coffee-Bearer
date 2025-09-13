#include "led_controller.h"

LEDController::LEDController() : 
    strip(nullptr),
    currentState(LED_OFF),
    currentPattern(PATTERN_SOLID),
    lastUpdate(0),
    animationStep(0),
    primaryColor(COLOR_OFF),
    secondaryColor(COLOR_OFF),
    initialized(false) {
}

LEDController::~LEDController() {
    end();
}

bool LEDController::begin() {
    if (initialized) {
        end();
    }
    
    strip = new Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
    
    if (!strip) {
        return false;
    }
    
    strip->begin();
    strip->setBrightness(128); // 50% brilho inicial
    strip->clear();
    strip->show();
    
    initialized = true;
    lastUpdate = millis();
    
    DEBUG_PRINTLN("LED Controller inicializado");
    return true;
}

void LEDController::end() {
    if (strip) {
        strip->clear();
        strip->show();
        delete strip;
        strip = nullptr;
    }
    initialized = false;
}

void LEDController::setOff() {
    currentState = LED_OFF;
    primaryColor = COLOR_OFF;
    currentPattern = PATTERN_SOLID;
    animationStep = 0;
}

void LEDController::setReady() {
    currentState = LED_READY;
    primaryColor = COLOR_READY;
    currentPattern = PATTERN_PULSE;
    animationStep = 0;
}

void LEDController::setServing() {
    currentState = LED_SERVING;
    primaryColor = COLOR_SERVING;
    currentPattern = PATTERN_CHASE;
    animationStep = 0;
}

void LEDController::setError() {
    currentState = LED_ERROR;
    primaryColor = COLOR_ERROR;
    currentPattern = PATTERN_PULSE;
    animationStep = 0;
}

void LEDController::setEmpty() {
    currentState = LED_EMPTY;
    primaryColor = COLOR_EMPTY;
    currentPattern = PATTERN_PULSE;
    animationStep = 0;
}

void LEDController::setConnecting() {
    currentState = LED_CONNECTING;
    primaryColor = COLOR_CONNECTING;
    currentPattern = PATTERN_CHASE;
    animationStep = 0;
}

void LEDController::setInitializing() {
    currentState = LED_INITIALIZING;
    primaryColor = COLOR_INITIALIZING;
    currentPattern = PATTERN_FADE;
    animationStep = 0;
}

void LEDController::setSuccess() {
    currentState = LED_SUCCESS;
    primaryColor = COLOR_SUCCESS;
    currentPattern = PATTERN_SOLID;
    animationStep = 0;
}

void LEDController::setCustom(uint32_t color, LEDPattern pattern) {
    primaryColor = color;
    currentPattern = pattern;
    animationStep = 0;
}

void LEDController::setCustomDual(uint32_t color1, uint32_t color2, LEDPattern pattern) {
    primaryColor = color1;
    secondaryColor = color2;
    currentPattern = pattern;
    animationStep = 0;
}

void LEDController::startRainbow() {
    currentPattern = PATTERN_RAINBOW;
    animationStep = 0;
}

void LEDController::showProgress(uint8_t percentage) {
    if (!initialized || !strip) return;
    
    int ledsToLight = map(percentage, 0, 100, 0, NEOPIXEL_COUNT);
    uint32_t color = colorHSV(map(percentage, 0, 100, 0, 85), 255, 255); // Verde -> Amarelo -> Vermelho
    
    strip->clear();
    for (int i = 0; i < ledsToLight; i++) {
        strip->setPixelColor(i, color);
    }
    strip->show();
}

void LEDController::flashColor(uint32_t color, int count) {
    if (!initialized || !strip) return;
    
    for (int i = 0; i < count; i++) {
        strip->fill(color);
        strip->show();
        delay(200);
        strip->clear();
        strip->show();
        delay(200);
    }
}

void LEDController::setBrightness(uint8_t brightness) {
    if (!initialized || !strip) return;
    strip->setBrightness(brightness);
}

uint8_t LEDController::getBrightness() {
    if (!initialized || !strip) return 0;
    return strip->getBrightness();
}

void LEDController::update() {
    if (!initialized || !strip) return;
    
    unsigned long currentTime = millis();
    if (currentTime - lastUpdate < LED_ANIMATION_SPEED) {
        return;
    }
    
    lastUpdate = currentTime;
    
    switch (currentPattern) {
        case PATTERN_SOLID:
            updateSolid();
            break;
        case PATTERN_PULSE:
            updatePulse();
            break;
        case PATTERN_CHASE:
            updateChase();
            break;
        case PATTERN_RAINBOW:
            updateRainbow();
            break;
        case PATTERN_FADE:
            updateFade();
            break;
    }
    
    strip->show();
}

void LEDController::updateSolid() {
    strip->fill(primaryColor);
}

void LEDController::updatePulse() {
    uint8_t brightness = sine8(animationStep * 5);
    uint32_t color = gamma32(primaryColor);
    
    // Aplicar brilho pulsante
    uint8_t r, g, b;
    getRGB(color, &r, &g, &b);
    
    r = (r * brightness) / 255;
    g = (g * brightness) / 255;
    b = (b * brightness) / 255;
    
    strip->fill(colorRGB(r, g, b));
    
    animationStep++;
    if (animationStep >= LED_PULSE_STEPS) {
        animationStep = 0;
    }
}

void LEDController::updateChase() {
    strip->clear();
    
    for (int i = 0; i < 3; i++) {
        int pos = (animationStep + i * (NEOPIXEL_COUNT / 3)) % NEOPIXEL_COUNT;
        strip->setPixelColor(pos, primaryColor);
    }
    
    animationStep++;
    if (animationStep >= NEOPIXEL_COUNT) {
        animationStep = 0;
    }
}

void LEDController::updateRainbow() {
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        int pixelHue = (animationStep + (i * 65536L / NEOPIXEL_COUNT));
        strip->setPixelColor(i, strip->gamma32(strip->ColorHSV(pixelHue)));
    }
    
    animationStep += 256;
    if (animationStep >= 65536) {
        animationStep = 0;
    }
}

void LEDController::updateFade() {
    uint8_t fade = animationStep < LED_FADE_STEPS ? 
                   map(animationStep, 0, LED_FADE_STEPS - 1, 0, 255) :
                   map(animationStep, LED_FADE_STEPS, LED_FADE_STEPS * 2 - 1, 255, 0);
    
    uint8_t r, g, b;
    getRGB(primaryColor, &r, &g, &b);
    
    r = (r * fade) / 255;
    g = (g * fade) / 255;
    b = (b * fade) / 255;
    
    strip->fill(colorRGB(r, g, b));
    
    animationStep++;
    if (animationStep >= LED_FADE_STEPS * 2) {
        animationStep = 0;
    }
}

uint32_t LEDController::wheel(byte wheelPos) {
    wheelPos = 255 - wheelPos;
    if (wheelPos < 85) {
        return colorRGB(255 - wheelPos * 3, 0, wheelPos * 3);
    }
    if (wheelPos < 170) {
        wheelPos -= 85;
        return colorRGB(0, wheelPos * 3, 255 - wheelPos * 3);
    }
    wheelPos -= 170;
    return colorRGB(wheelPos * 3, 255 - wheelPos * 3, 0);
}

uint32_t LEDController::gamma32(uint32_t color) {
    if (!strip) return color;
    return strip->gamma32(color);
}

uint32_t LEDController::blend(uint32_t color1, uint32_t color2, uint8_t blend) {
    uint8_t r1, g1, b1, r2, g2, b2;
    getRGB(color1, &r1, &g1, &b1);
    getRGB(color2, &r2, &g2, &b2);
    
    uint8_t r = ((r1 * (255 - blend)) + (r2 * blend)) / 255;
    uint8_t g = ((g1 * (255 - blend)) + (g2 * blend)) / 255;
    uint8_t b = ((b1 * (255 - blend)) + (b2 * blend)) / 255;
    
    return colorRGB(r, g, b);
}

uint8_t LEDController::sine8(uint8_t x) {
    const uint8_t PROGMEM sine_table[256] = {
        128,131,134,137,140,143,146,149,152,155,158,162,165,167,170,173,
        176,179,182,185,188,190,193,196,198,201,203,206,208,211,213,215,
        218,220,222,224,226,228,230,232,234,235,237,238,240,241,243,244,
        245,246,248,249,250,250,251,252,253,253,254,254,254,255,255,255,
        255,255,255,255,254,254,254,253,253,252,251,250,250,249,248,246,
        245,244,243,241,240,238,237,235,234,232,230,228,226,224,222,220,
        218,215,213,211,208,206,203,201,198,196,193,190,188,185,182,179,
        176,173,170,167,165,162,158,155,152,149,146,143,140,137,134,131,
        128,124,121,118,115,112,109,106,103,100,97,93,90,88,85,82,
        79,76,73,70,67,65,62,59,57,54,52,49,47,44,42,40,
        37,35,33,31,29,27,25,23,21,20,18,17,15,14,12,11,
        10,9,7,6,5,5,4,3,2,2,1,1,1,0,0,0,
        0,0,0,0,1,1,1,2,2,3,4,5,5,6,7,9,
        10,11,12,14,15,17,18,20,21,23,25,27,29,31,33,35,
        37,40,42,44,47,49,52,54,57,59,62,65,67,70,73,76,
        79,82,85,88,90,93,97,100,103,106,109,112,115,118,121,124
    };
    return pgm_read_byte(&sine_table[x]);
}

uint32_t LEDController::colorRGB(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t LEDController::colorHSV(uint16_t hue, uint8_t sat, uint8_t val) {
    uint8_t r, g, b;
    
    // Normalizar hue para 0-1535
    hue = (hue * 1536L) / 360;
    if (hue >= 1536) hue -= 1536;
    
    uint8_t sector = hue / 256;
    uint8_t offset = hue % 256;
    
    uint8_t p = (val * (255 - sat)) / 255;
    uint8_t q = (val * (255 - ((sat * offset) / 255))) / 255;
    uint8_t t = (val * (255 - ((sat * (255 - offset)) / 255))) / 255;
    
    switch (sector) {
        case 0: r = val; g = t;   b = p;   break;
        case 1: r = q;   g = val; b = p;   break;
        case 2: r = p;   g = val; b = t;   break;
        case 3: r = p;   g = q;   b = val; break;
        case 4: r = t;   g = p;   b = val; break;
        case 5: r = val; g = p;   b = q;   break;
        default: r = 0; g = 0; b = 0; break;
    }
    
    return colorRGB(r, g, b);
}

void LEDController::getRGB(uint32_t color, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}