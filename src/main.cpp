/**
 * @file main.cpp
 * @brief Monitor de Qualidade de Energia (IEC 61000-4-30: Sag & Swell)
 * @author Engenharia de Sistemas Embarcados & IoT
 * @target ESP32 Dual-Core Xtensa LX6 (Arduino Framework / FreeRTOS)
 *
 * Arquitetura Dual-Core:
 *  - Core 1: Amostragem contínua a 3840 Hz (64 amostras/ciclo @ 60 Hz), cálculo RMS,
 *            normalização pu, máquina de estados com histerese e enfileiramento de eventos.
 *  - Core 0: Pilha Wi-Fi (SoftAP/STA), ESPAsyncWebServer, WebSocket em tempo real (/ws),
 *            REST API e transmissão de pacotes JSON para a interface gráfica.
 */

#include <Arduino.h>
#include <WiFi.h>
#include "types.h"
#include "dsp_engine.h"
#include "web_server.h"

// ==========================================
// Configurações de Conexão Wi-Fi
// ==========================================
// Configuração padrão: Ponto de Acesso (SoftAP) autônomo e imediato
#define WIFI_AP_SSID     "ESP32-PowerQuality"
#define WIFI_AP_PASSWORD "metrum123"

// Opcional: Para conectar a um roteador existente, defina USE_STA_MODE como true
#define USE_STA_MODE     false
#define WIFI_STA_SSID    "SuaRedeWifi"
#define WIFI_STA_PASS    "SuaSenhaWifi"

// ==========================================
// Objetos Globais & Primitivas FreeRTOS
// ==========================================
PowerQualityConfig  systemConfig;
SemaphoreHandle_t   configMutex   = nullptr;
QueueHandle_t       telemetryQueue = nullptr;
QueueHandle_t       eventQueue     = nullptr;

DSPEngine           dspEngine;
WebServerManager    webServer;

TaskHandle_t        dspTaskHandle = nullptr;
TaskHandle_t        webTaskHandle = nullptr;

// ==========================================
// Tarefa Core 1: DSP, Amostragem e Máquina de Estados
// ==========================================
void dspTask(void *pvParameters) {
    Serial.printf("[Core 1] Tarefa DSP iniciada no Core %d (Prioridade: %d)\n", 
                  xPortGetCoreID(), uxTaskPriorityGet(NULL));

    // Inicializa o motor de processamento com as filas e ponteiro de configuração
    dspEngine.begin(telemetryQueue, eventQueue, configMutex, &systemConfig);

    // Loop de amostragem síncrono e contínuo
    for (;;) {
        dspEngine.runCycle();
    }
}

// ==========================================
// Tarefa Core 0: Wi-Fi, Servidor Web e WebSockets
// ==========================================
void webTask(void *pvParameters) {
    Serial.printf("[Core 0] Tarefa Web iniciada no Core %d (Prioridade: %d)\n", 
                  xPortGetCoreID(), uxTaskPriorityGet(NULL));

    // Inicializa o Servidor Web Assíncrono e WebSocket
    webServer.begin(telemetryQueue, eventQueue, configMutex, &systemConfig);

    // Loop de atendimento e broadcast assíncrono (~20 Hz)
    for (;;) {
        webServer.update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ==========================================
// Inicialização do Wi-Fi (SoftAP / STA)
// ==========================================
void setupWiFi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    if (USE_STA_MODE) {
        Serial.printf("[WiFi] Conectando à rede: %s ...\n", WIFI_STA_SSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Conectado em modo STA! IP: %s\n", WiFi.localIP().toString().c_str());
            return;
        }
        Serial.println("[WiFi] Falha ao conectar em modo STA. Iniciando SoftAP fallback...");
    }

    // Inicializa Ponto de Acesso (SoftAP)
    WiFi.mode(WIFI_AP);
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_ip, gateway, subnet);

    bool apCreated = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    if (apCreated) {
        Serial.printf("[WiFi] SoftAP iniciado com sucesso!\n");
        Serial.printf("[WiFi] SSID: %s | Senha: %s\n", WIFI_AP_SSID, WIFI_AP_PASSWORD);
        Serial.printf("[WiFi] Acesse o Dashboard em: http://%s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[WiFi] Erro ao iniciar SoftAP!");
    }
}

// ==========================================
// Setup Principal
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=======================================================");
    Serial.println("  METRUM / HackatOhm - Monitor de Qualidade de Energia");
    Serial.println("  IEC 61000-4-30: Detector de Sag & Swell (60 Hz)");
    Serial.println("  Arquitetura Dual-Core FreeRTOS no ESP32");
    Serial.println("=======================================================\n");

    // 1. Criação das Primitivas de Sincronização FreeRTOS
    configMutex = xSemaphoreCreateMutex();
    if (configMutex == nullptr) {
        Serial.println("[FATAL] Falha ao criar mutex de configuração!");
        while (1) delay(1000);
    }

    // Fila de Telemetria: Tamanho 1 com xQueueOverwrite (não bloqueante)
    telemetryQueue = xQueueCreate(1, sizeof(TelemetryData));

    // Fila de Eventos Consolidados: Buffer de até 32 eventos sem perda
    eventQueue = xQueueCreate(32, sizeof(PowerQualityEvent));

    if (telemetryQueue == nullptr || eventQueue == nullptr) {
        Serial.println("[FATAL] Falha ao criar Filas do FreeRTOS!");
        while (1) delay(1000);
    }

    // 2. Inicialização da Conectividade Wi-Fi
    setupWiFi();

    // 3. Criação da Tarefa DSP no Core 1 (Prioridade Máxima em Tempo Real)
    // Core 1 é dedicado exclusivamente à aquisição analógica a 3840 Hz e DSP
    xTaskCreatePinnedToCore(
        dspTask,
        "DSP_Core1_Task",
        4096,               // Stack size em words (16 KB)
        NULL,
        configMAX_PRIORITIES - 1, // Prioridade mais alta
        &dspTaskHandle,
        1                   // Pinned to Core 1
    );

    // 4. Criação da Tarefa de Comunicação e Web no Core 0 (Prioridade Baixa/Média)
    // Core 0 executa a pilha Wi-Fi, servidor HTTP assíncrono e WebSockets
    xTaskCreatePinnedToCore(
        webTask,
        "Web_Core0_Task",
        8192,               // Stack size em words (32 KB para pilha de rede)
        NULL,
        2,                  // Prioridade baixa/média
        &webTaskHandle,
        0                   // Pinned to Core 0
    );

    Serial.println("[Sistema] Todas as tarefas FreeRTOS foram instanciadas com sucesso.");
}

// ==========================================
// Loop do Arduino (Vazio / Deletado para economia de recursos)
// ==========================================
void loop() {
    // O loop padrão do Arduino executa no Core 1 com prioridade 1.
    // Deletamos a tarefa do loop para liberar recursos e garantir que o Core 1
    // fique 100% sob controle determinístico da nossa dspTask.
    vTaskDelete(NULL);
}
