#ifndef DSP_ENGINE_H
#define DSP_ENGINE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "types.h"

// Configurações de amostragem conforme IEC 61000-4-30 (Rede 60 Hz)
#define GRID_FREQ_HZ         60.0f
#define SAMPLES_PER_CYCLE    64
#define CYCLE_PERIOD_US      16667   // 1 / 60 Hz ~ 16.667 ms
#define SAMPLE_INTERVAL_US   260     // ~260.417 µs por amostra (3840 Hz)
#define ADC_PIN              34      // GPIO 34 (ADC1_CH6 - Input Only)
#define TELEMETRY_DECIMATION 6       // Envia telemetria a cada 6 ciclos (~10 Hz)

class DSPEngine {
public:
    DSPEngine();
    void begin(QueueHandle_t telemetryQ, QueueHandle_t eventQ, SemaphoreHandle_t cfgMutex, PowerQualityConfig* cfgPtr);
    void runCycle();

    // Métodos para controle de simulação
    void setSimulation(bool enabled, float pu);

private:
    QueueHandle_t     _telemetryQueue;
    QueueHandle_t     _eventQueue;
    SemaphoreHandle_t _configMutex;
    PowerQualityConfig* _config;

    // Buffers de amostragem
    uint16_t _sampleBuffer[SAMPLES_PER_CYCLE];
    uint32_t _cycleCounter;
    uint32_t _eventCounter;

    // Variáveis da Máquina de Estados
    PowerQualityState _currentState;
    uint32_t _eventStartTimeMs;
    uint32_t _eventStartCycle;
    float    _extremePu;
    float    _extremeVoltage;

    // Métodos internos
    void sampleCycle();
    float computeRMS();
    void processStateMachine(float v_rms, float pu);
};

#endif // DSP_ENGINE_H
