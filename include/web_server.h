#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "types.h"

#define MAX_HISTORICAL_EVENTS 64
#define WS_SEND_BUFFER_SIZE   512

class WebServerManager {
public:
    WebServerManager();
    void begin(QueueHandle_t telemetryQ, QueueHandle_t eventQ, SemaphoreHandle_t cfgMutex, PowerQualityConfig* cfgPtr);
    void update(); // Chamado dentro do loop da Task do Core 0

    // Controle de simulação via Web
    void triggerSimulation(float pu, uint32_t durationMs = 0);
    void stopSimulation();

private:
    AsyncWebServer   _server;
    AsyncWebSocket   _ws;
    QueueHandle_t    _telemetryQueue;
    QueueHandle_t    _eventQueue;
    SemaphoreHandle_t _configMutex;
    PowerQualityConfig* _config;

    // Histórico de eventos consolidado em memória circular
    PowerQualityEvent _eventHistory[MAX_HISTORICAL_EVENTS];
    int _eventCount;
    int _eventHead;

    // Gerenciamento de simulação temporizada
    bool     _simTimerActive;
    uint32_t _simEndTimeMs;

    void setupRoutes();
    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void broadcastTelemetry(const TelemetryData& telem);
    void broadcastEvent(const PowerQualityEvent& evt);
    void addEventToHistory(const PowerQualityEvent& evt);
};

#endif // WEB_SERVER_H
