#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <Arduino.h>
#include <ArduinoWebSockets.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "types.h"

/**
 * @brief Gerenciador de Comunicação WebSocket Client para o Core 0 do ESP32
 *        Conecta ao Servidor Central Python na porta 3000
 */
class WSClientManager {
public:
    WSClientManager();
    void begin(const char* serverHost, uint16_t serverPort, const char* deviceId,
               QueueHandle_t telemetryQ, QueueHandle_t eventQ, 
               SemaphoreHandle_t cfgMutex, PowerQualityConfig* cfgPtr);
    void update();
    bool isConnected();

    void triggerSimulation(float pu, uint32_t durationMs = 0);
    void stopSimulation();

private:
    websockets::WebsocketsClient _client;
    const char*       _serverHost;
    uint16_t          _serverPort;
    String            _deviceId;
    String            _path;
    QueueHandle_t     _telemetryQueue;
    QueueHandle_t     _eventQueue;
    SemaphoreHandle_t _configMutex;
    PowerQualityConfig* _config;

    unsigned long     _lastConnectAttempt;
    bool              _simTimerActive;
    unsigned long     _simEndTimeMs;

    void connectToServer();
    void handleIncomingMessage(const String& payload);
    void sendTelemetry(const TelemetryData& telem);
    void sendEvent(const PowerQualityEvent& evt);
};

#endif // WS_CLIENT_H
