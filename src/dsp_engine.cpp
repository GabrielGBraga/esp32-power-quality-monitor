#include "dsp_engine.h"
#include <math.h>

DSPEngine::DSPEngine()
    : _telemetryQueue(nullptr),
      _eventQueue(nullptr),
      _configMutex(nullptr),
      _config(nullptr),
      _cycleCounter(0),
      _eventCounter(0),
      _currentState(STATE_NORMAL),
      _eventStartTimeMs(0),
      _eventStartCycle(0),
      _extremePu(1.0f),
      _extremeVoltage(127.0f)
{
    memset(_sampleBuffer, 0, sizeof(_sampleBuffer));
}

void DSPEngine::begin(QueueHandle_t telemetryQ, QueueHandle_t eventQ, SemaphoreHandle_t cfgMutex, PowerQualityConfig* cfgPtr) {
    _telemetryQueue = telemetryQ;
    _eventQueue = eventQ;
    _configMutex = cfgMutex;
    _config = cfgPtr;

    // Configuração do ADC do ESP32 (ADC1_CH6 - GPIO 34)
    // 12 bits de resolução (0 a 4095), atenuação de 11dB (~0 - 3.3V)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    pinMode(ADC_PIN, INPUT);
}

void DSPEngine::setSimulation(bool enabled, float pu) {
    if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _config->simulation_mode = enabled;
        _config->sim_voltage_pu = pu;
        xSemaphoreGive(_configMutex);
    }
}

void DSPEngine::sampleCycle() {
    // Obter configuração de forma rápida e segura
    bool simMode = false;
    float simPu = 1.0f;
    if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        simMode = _config->simulation_mode;
        simPu = _config->sim_voltage_pu;
        xSemaphoreGive(_configMutex);
    }

    int64_t nextSampleTime = esp_timer_get_time();

    for (int k = 0; k < SAMPLES_PER_CYCLE; ++k) {
        if (!simMode) {
            // Amostragem do sinal analógico real no GPIO 34
            _sampleBuffer[k] = analogRead(ADC_PIN);
        } else {
            // Modo Simulação Didática: Sintetiza senóide pura de 60 Hz com amplitude controlada
            // Ponto médio DC em 2048 (~1.65V), amplitude nominal pico de 1400 (~127V RMS)
            float phase = (2.0f * (float)M_PI * (float)k) / (float)SAMPLES_PER_CYCLE;
            float amplitude = 1400.0f * simPu;
            // Ruído branco gaussiano simulado leve para realismo industrial (+- 4 LSB)
            int noise = (rand() % 9) - 4;
            int sample = (int)(2048.0f + amplitude * sinf(phase) + noise);
            if (sample < 0) sample = 0;
            if (sample > 4095) sample = 4095;
            _sampleBuffer[k] = (uint16_t)sample;
        }

        nextSampleTime += SAMPLE_INTERVAL_US;
        // Pacing de alta precisão em microssegundos no Core 1
        while (esp_timer_get_time() < nextSampleTime) {
            asm volatile("nop");
        }
    }
}

float DSPEngine::computeRMS() {
    // 1. Cálculo da média amostral para remoção do offset DC (típico 1.65V / 2048 LSB)
    float sum = 0.0f;
    for (int i = 0; i < SAMPLES_PER_CYCLE; ++i) {
        sum += (float)_sampleBuffer[i];
    }
    float mean = sum / (float)SAMPLES_PER_CYCLE;

    // 2. Cálculo da soma dos quadrados dos desvios (componente puramente AC)
    float sumSquares = 0.0f;
    for (int i = 0; i < SAMPLES_PER_CYCLE; ++i) {
        float ac = (float)_sampleBuffer[i] - mean;
        sumSquares += (ac * ac);
    }

    // 3. Valor RMS bruto em LSB
    float rmsRaw = sqrtf(sumSquares / (float)SAMPLES_PER_CYCLE);

    // 4. Conversão para Volts físicos considerando calibração e v_ref
    float vRms = 0.0f;
    if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (_config->simulation_mode) {
            // No modo de simulação, calibração ajustada para 1400 pico = 127V RMS
            // 1400 / sqrt(2) = 989.95 LSB RMS -> fator = v_ref / 989.95
            float calFactor = _config->v_ref / 989.95f;
            vRms = rmsRaw * calFactor;
        } else {
            vRms = rmsRaw * _config->adc_calibration;
        }
        xSemaphoreGive(_configMutex);
    } else {
        // Fallback seguro caso o mutex esteja temporariamente ocupado
        vRms = rmsRaw * 0.12829f;
    }

    return vRms;
}

void DSPEngine::processStateMachine(float v_rms, float pu) {
    float sagThresh = 0.90f;
    float swellThresh = 1.10f;
    float hyst = 0.02f;

    if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        sagThresh = _config->sag_threshold;
        swellThresh = _config->swell_threshold;
        hyst = _config->hysteresis;
        xSemaphoreGive(_configMutex);
    }

    switch (_currentState) {
        case STATE_NORMAL:
            if (pu <= sagThresh) {
                // Início do evento de AFUNDAMENTO (SAG)
                _currentState = STATE_SAG;
                _eventStartTimeMs = millis();
                _eventStartCycle = _cycleCounter;
                _extremePu = pu;
                _extremeVoltage = v_rms;
                _eventCounter++;
            } else if (pu >= swellThresh) {
                // Início do evento de ELEVAÇÃO (SWELL)
                _currentState = STATE_SWELL;
                _eventStartTimeMs = millis();
                _eventStartCycle = _cycleCounter;
                _extremePu = pu;
                _extremeVoltage = v_rms;
                _eventCounter++;
            }
            break;

        case STATE_SAG:
            // Rastreamento do ponto mais profundo (mínimo de tensão)
            if (pu < _extremePu) {
                _extremePu = pu;
                _extremeVoltage = v_rms;
            }

            // Critério de término IEC 61000-4-30 com histerese
            if (pu >= (sagThresh + hyst)) {
                PowerQualityEvent evt;
                evt.id = _eventCounter;
                evt.type = STATE_SAG;
                evt.timestamp_ms = _eventStartTimeMs;
                evt.duration_ms = millis() - _eventStartTimeMs;
                evt.duration_cycles = _cycleCounter - _eventStartCycle;
                evt.extreme_voltage = _extremeVoltage;
                evt.extreme_pu = _extremePu;

                // Enfileira evento consolidado para o Core 0 de forma não-bloqueante
                if (_eventQueue != nullptr) {
                    xQueueSend(_eventQueue, &evt, 0);
                }

                _currentState = STATE_NORMAL;
            }
            break;

        case STATE_SWELL:
            // Rastreamento do pico mais alto (máximo de tensão)
            if (pu > _extremePu) {
                _extremePu = pu;
                _extremeVoltage = v_rms;
            }

            // Critério de término IEC 61000-4-30 com histerese
            if (pu <= (swellThresh - hyst)) {
                PowerQualityEvent evt;
                evt.id = _eventCounter;
                evt.type = STATE_SWELL;
                evt.timestamp_ms = _eventStartTimeMs;
                evt.duration_ms = millis() - _eventStartTimeMs;
                evt.duration_cycles = _cycleCounter - _eventStartCycle;
                evt.extreme_voltage = _extremeVoltage;
                evt.extreme_pu = _extremePu;

                // Enfileira evento consolidado para o Core 0 de forma não-bloqueante
                if (_eventQueue != nullptr) {
                    xQueueSend(_eventQueue, &evt, 0);
                }

                _currentState = STATE_NORMAL;
            }
            break;
    }
}

void DSPEngine::runCycle() {
    _cycleCounter++;

    // 1. Amostragem contínua de 64 amostras por ciclo (~16.67 ms)
    sampleCycle();

    // 2. Cálculo do RMS real no ciclo atual
    float vRms = computeRMS();

    // 3. Normalização em por unidade (pu = V_RMS / V_ref)
    float vRef = 127.0f;
    if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        vRef = _config->v_ref;
        xSemaphoreGive(_configMutex);
    }
    float pu = (vRef > 0.0f) ? (vRms / vRef) : 1.0f;

    // 4. Processamento da Máquina de Estados com Histerese
    processStateMachine(vRms, pu);

    // 5. Decimação de Telemetria: Envia pacote leve para o Core 0 a ~10 Hz (a cada 6 ciclos)
    if ((_cycleCounter % TELEMETRY_DECIMATION) == 0 && _telemetryQueue != nullptr) {
        TelemetryData telem;
        telem.timestamp_ms = millis();
        telem.v_rms = vRms;
        telem.pu = pu;
        telem.frequency = GRID_FREQ_HZ;
        telem.state = _currentState;
        telem.current_cycle = _cycleCounter;
        telem.active_event_duration_ms = (_currentState != STATE_NORMAL) ? (millis() - _eventStartTimeMs) : 0;
        telem.active_event_extreme_pu = (_currentState != STATE_NORMAL) ? _extremePu : pu;

        // Sobrescreve pacote anterior para garantir dado sempre fresco sem acúmulo na fila
        xQueueOverwrite(_telemetryQueue, &telem);
    }

    // Permite que o IDLE task do Core 1 execute para alimentar o Task Watchdog Timer
    taskYIELD();
}
