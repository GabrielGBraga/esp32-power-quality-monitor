/**
 * @file main.cpp
 * @brief Sonda IoT ESP32 - Monitor de Qualidade de Energia (IEC 61000-4-30: Sag & Swell)
 * @author Engenharia de Sistemas Embarcados & IoT (Metrum / HackatOhm)
 * @target ESP32 Dual-Core Xtensa LX6
 *
 * Arquitetura Desacoplada:
 *  - Core 1: DSP & Amostragem Determinística (64 amostras/ciclo @ 60 Hz = 3840 Hz) no GPIO 34.
 *            Cálculo True RMS, normalização pu e máquina de estados com histerese.
 *  - Core 0: Cliente IoT WebSocket conectado ao Servidor Central Python na porta 3000.
 *            Transmite telemetria periódica (~10 Hz), eventos consolidados e recebe comandos.
 */

#include <Arduino.h>
#include <WiFi.h>
#include "types.h"
#include "dsp_engine.h"
#include "ws_client.h"

// ==========================================
// Configurações de Rede e do Servidor Central
// ==========================================
// Credenciais da rede Wi-Fi local onde o servidor Python está executando
#define WIFI_SSID         "SuaRedeWifi"
#define WIFI_PASSWORD     "SuaSenhaWifi"

// Endereço IP e porta do Servidor Central Python (ex: IP do seu notebook/PC na rede)
#define SERVER_HOST       "192.168.1.100"
#define SERVER_PORT       3000

// Identificador único desta sonda (permite múltiplos ESP32s no mesmo servidor)
#define DEVICE_ID         "ESP32-SUBESTACAO-01"

// Se não conseguir conectar à rede STA, inicia SoftAP de contingência
#define FALLBACK_AP_SSID  "ESP32-PQ-CONFIG"
#define FALLBACK_AP_PASS  "metrum123"

// ==========================================
// Objetos Globais & Primitivas FreeRTOS
// ==========================================
PowerQualityConfig  systemConfig;
SemaphoreHandle_t   configMutex    = nullptr;
QueueHandle_t       telemetryQueue = nullptr;
QueueHandle_t       eventQueue     = nullptr;

DSPEngine           dspEngine;
WSClientManager     wsClient;

TaskHandle_t        dspTaskHandle  = nullptr;
TaskHandle_t        commTaskHandle = nullptr;

// ==========================================
// Tarefa Core 1: DSP, Amostragem e IEC 61000-4-30
// ==========================================
void dspTask(void *pvParameters) {
    Serial.printf("[Core 1] Tarefa DSP iniciada no Core %d (Prioridade: %d)\n", 
                  xPortGetCoreID(), uxTaskPriorityGet(NULL));

    // Inicializa o motor analógico com as filas e ponteiro de configuração
    dspEngine.begin(telemetryQueue, eventQueue, configMutex, &systemConfig);

    // Loop de amostragem síncrono e contínuo a 3840 Hz
    for (;;) {
        dspEngine.runCycle();
    }
}

// ==========================================
// Tarefa Core 0: Cliente WebSocket e Rede
// ==========================================
void commTask(void *pvParameters) {
    Serial.printf("[Core 0] Tarefa de Comunicação iniciada no Core %d (Prioridade: %d)\n", 
                  xPortGetCoreID(), uxTaskPriorityGet(NULL));

    // Inicializa o cliente WebSocket apontando para o servidor central
    wsClient.begin(SERVER_HOST, SERVER_PORT, DEVICE_ID, telemetryQueue, eventQueue, configMutex, &systemConfig);

    // Loop de transmissão e recepção assíncrona (~20 Hz)
    for (;;) {
        wsClient.update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ==========================================
// Inicialização do Wi-Fi
// ==========================================
void setupWiFi() {
    Serial.printf("[WiFi] Conectando à rede: %s ...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 8000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Conectado! IP do ESP32: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] Rede principal não encontrada. Ativando Ponto de Acesso de emergência...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASS);
        Serial.printf("[WiFi] SoftAP ativo: %s (IP: 192.168.4.1)\n", FALLBACK_AP_SSID);
    }
}

// ==========================================
// Setup Principal
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=======================================================");
    Serial.println("  METRUM / HackatOhm - Sonda IoT ESP32 (IEC 61000-4-30)");
    Serial.println("  Transmissão Multi-Sonda para Servidor Python :3000");
    Serial.printf("  Device ID: %s | Alvo: ws://%s:%u\n", DEVICE_ID, SERVER_HOST, SERVER_PORT);
    Serial.println("=======================================================\n");

    // 1. Criação das Primitivas de Sincronização FreeRTOS
    configMutex = xSemaphoreCreateMutex();
    if (configMutex == nullptr) {
        Serial.println("[FATAL] Falha ao criar mutex de configuração!");
        while (1) delay(1000);
    }

    // Fila de Telemetria: Tamanho 1 com sobrescrita não bloqueante
    telemetryQueue = xQueueCreate(1, sizeof(TelemetryData));

    // Fila de Eventos Consolidados: Buffer para 32 eventos sem perda
    eventQueue = xQueueCreate(32, sizeof(PowerQualityEvent));

    if (telemetryQueue == nullptr || eventQueue == nullptr) {
        Serial.println("[FATAL] Falha ao criar Filas do FreeRTOS!");
        while (1) delay(1000);
    }

    // 2. Conexão Wi-Fi
    setupWiFi();

    // 3. Tarefa DSP no Core 1 (Prioridade Máxima em Tempo Real)
    xTaskCreatePinnedToCore(
        dspTask,
        "DSP_Core1_Task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,
        &dspTaskHandle,
        1
    );

    // 4. Tarefa de Comunicação no Core 0 (Prioridade Baixa/Média)
    xTaskCreatePinnedToCore(
        commTask,
        "Comm_Core0_Task",
        8192,
        NULL,
        2,
        &commTaskHandle,
        0
    );

    Serial.println("[Sistema] Sonda IoT e Tarefas FreeRTOS iniciadas com sucesso.");
}

void loop() {
    // Loop deletado para economizar recursos no Core 1
    vTaskDelete(NULL);
}
