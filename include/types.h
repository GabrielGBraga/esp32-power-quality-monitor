#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

/**
 * @brief Estados operacionais da máquina de estados IEC 61000-4-30
 */
enum PowerQualityState : uint8_t {
    STATE_NORMAL = 0,
    STATE_SAG    = 1,   // Afundamento de Tensão
    STATE_SWELL  = 2    // Elevação de Tensão
};

/**
 * @brief Estrutura de Configuração Dinâmica dos Limiares e Parâmetros
 */
struct PowerQualityConfig {
    float v_ref           = 127.0f; // Tensão nominal de referência em Volts RMS
    float sag_threshold   = 0.90f;  // Limiar de início de Sag (pu)
    float swell_threshold = 1.10f;  // Limiar de início de Swell (pu)
    float hysteresis      = 0.02f;  // Histerese de desarme/retorno (pu)
    float adc_calibration = 0.125f; // Fator de escala ADC -> Volts
    bool  simulation_mode = false;  // Permite injetar distúrbios virtuais para testes
    float sim_voltage_pu  = 1.00f;  // Tensão forçada durante teste de simulação
};

/**
 * @brief Pacote de Telemetria Periódica (~10 Hz) transmitido via WebSocket
 */
struct TelemetryData {
    uint32_t timestamp_ms;
    float    v_rms;
    float    pu;
    float    frequency;
    PowerQualityState state;
    uint32_t current_cycle;
    uint32_t active_event_duration_ms;
    float    active_event_extreme_pu;
};

/**
 * @brief Evento consolidado de Qualidade de Energia (Sag / Swell)
 *        Em conformidade com a IEC 61000-4-30
 */
struct PowerQualityEvent {
    uint32_t id;
    PowerQualityState type;
    uint32_t timestamp_ms;
    uint32_t duration_ms;
    uint32_t duration_cycles;
    float    extreme_voltage;
    float    extreme_pu;
};

#endif // TYPES_H
