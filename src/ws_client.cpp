#include "ws_client.h"

WSClientManager::WSClientManager()
    : _serverHost("192.168.1.100"),
      _serverPort(3000),
      _deviceId("ESP32-PQ-01"),
      _telemetryQueue(nullptr),
      _eventQueue(nullptr),
      _configMutex(nullptr),
      _config(nullptr),
      _lastConnectAttempt(0),
      _simTimerActive(false),
      _simEndTimeMs(0)
{
}

void WSClientManager::begin(const char* serverHost, uint16_t serverPort, const char* deviceId,
                           QueueHandle_t telemetryQ, QueueHandle_t eventQ, 
                           SemaphoreHandle_t cfgMutex, PowerQualityConfig* cfgPtr) {
    _serverHost = serverHost;
    _serverPort = serverPort;
    _deviceId = String(deviceId);
    _path = String("/ws/device?device_id=") + _deviceId;
    _telemetryQueue = telemetryQ;
    _eventQueue = eventQ;
    _configMutex = cfgMutex;
    _config = cfgPtr;

    _client.onMessage([this](websockets::WebsocketsMessage message) {
        this->handleIncomingMessage(message.data());
    });

    _client.onEvent([](websockets::WebsocketsEvent event, String data) {
        if (event == websockets::WebsocketsEvent::ConnectionOpened) {
            Serial.println("[WS-Client] Conexão estabelecida com o Servidor Python na porta 3000!");
        } else if (event == websockets::WebsocketsEvent::ConnectionClosed) {
            Serial.println("[WS-Client] Desconectado do Servidor Python.");
        }
    });

    connectToServer();
}

void WSClientManager::connectToServer() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    Serial.printf("[WS-Client] Conectando a ws://%s:%u%s ...\n", _serverHost, _serverPort, _path.c_str());
    bool connected = _client.connect(_serverHost, _serverPort, _path.c_str());
    if (connected) {
        Serial.printf("[WS-Client] Sucesso! Sonda '%s' online no Servidor Central.\n", _deviceId.c_str());
    } else {
        Serial.println("[WS-Client] Falha na conexão. Nova tentativa em 3s...");
    }
}

bool WSClientManager::isConnected() {
    return _client.available();
}

void WSClientManager::handleIncomingMessage(const String& payload) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[WS-Client] Erro ao parsear JSON recebido: %s\n", err.c_str());
        return;
    }

    String type = doc["type"] | "";

    if (type == "config") {
        Serial.println("[WS-Client] Nova configuração recebida do Servidor Central:");
        if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (doc["v_ref"].is<float>()) _config->v_ref = doc["v_ref"].as<float>();
            if (doc["sag_threshold"].is<float>()) _config->sag_threshold = doc["sag_threshold"].as<float>();
            if (doc["swell_threshold"].is<float>()) _config->swell_threshold = doc["swell_threshold"].as<float>();
            if (doc["hysteresis"].is<float>()) _config->hysteresis = doc["hysteresis"].as<float>();
            xSemaphoreGive(_configMutex);
            Serial.printf("  Vref: %.1f V | Sag: %.2f pu | Swell: %.2f pu | Hyst: %.3f\n",
                _config->v_ref, _config->sag_threshold, _config->swell_threshold, _config->hysteresis);
        }
    } else if (type == "simulate") {
        String scenario = doc["scenario"] | "nominal";
        float pu = doc["pu"] | 1.0f;
        uint32_t duration = doc["duration_ms"] | 0;

        if (scenario == "adc") {
            stopSimulation();
            Serial.println("[WS-Client] Retornando ao Modo Real (ADC GPIO 34)");
        } else {
            triggerSimulation(pu, duration);
            Serial.printf("[WS-Client] Simulação disparada remotamente: %s (%.2f pu, %u ms)\n",
                scenario.c_str(), pu, duration);
        }
    }
}

void WSClientManager::triggerSimulation(float pu, uint32_t durationMs) {
    if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        _config->simulation_mode = true;
        _config->sim_voltage_pu = pu;
        xSemaphoreGive(_configMutex);
    }
    if (durationMs > 0) {
        _simTimerActive = true;
        _simEndTimeMs = millis() + durationMs;
    } else {
        _simTimerActive = false;
    }
}

void WSClientManager::stopSimulation() {
    if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        _config->simulation_mode = false;
        _config->sim_voltage_pu = 1.0f;
        xSemaphoreGive(_configMutex);
    }
    _simTimerActive = false;
}

void WSClientManager::sendTelemetry(const TelemetryData& telem) {
    if (!_client.available()) return;

    JsonDocument doc;
    doc["device_id"]      = _deviceId;
    doc["type"]           = "telemetry";
    doc["timestamp_ms"]   = telem.timestamp_ms;
    doc["v_rms"]          = telem.v_rms;
    doc["pu"]             = telem.pu;
    doc["freq"]           = telem.frequency;
    doc["state"]          = (telem.state == STATE_SAG) ? "SAG" : ((telem.state == STATE_SWELL) ? "SWELL" : "NORMAL");
    doc["cycle"]          = telem.current_cycle;
    doc["active_dur"]     = telem.active_event_duration_ms;
    doc["active_extreme"] = telem.active_event_extreme_pu;

    String json;
    serializeJson(doc, json);
    _client.send(json);
}

void WSClientManager::sendEvent(const PowerQualityEvent& evt) {
    if (!_client.available()) return;

    JsonDocument doc;
    doc["device_id"]       = _deviceId;
    doc["type"]            = "event";
    doc["id"]              = evt.id;
    doc["class"]           = (evt.type == STATE_SAG) ? "SAG" : "SWELL";
    doc["timestamp_ms"]    = evt.timestamp_ms;
    doc["duration_ms"]     = evt.duration_ms;
    doc["duration_cycles"] = evt.duration_cycles;
    doc["extreme_voltage"] = evt.extreme_voltage;
    doc["extreme_pu"]      = evt.extreme_pu;

    String json;
    serializeJson(doc, json);
    _client.send(json);

    Serial.printf("[PQ-EVENT TX] #%u %s enviado ao servidor central (%u ms, %.3f pu)\n",
        evt.id, (evt.type == STATE_SAG) ? "SAG" : "SWELL", evt.duration_ms, evt.extreme_pu);
}

void WSClientManager::update() {
    // 1. Gerenciamento de reconexão automática
    if (WiFi.status() == WL_CONNECTED) {
        if (!_client.available()) {
            if (millis() - _lastConnectAttempt >= 3000) {
                _lastConnectAttempt = millis();
                connectToServer();
            }
        } else {
            // Processa eventos e dados recebidos do WebSocket
            _client.poll();
        }
    }

    // 2. Temporizador para reversão de simulação
    if (_simTimerActive && millis() >= _simEndTimeMs) {
        triggerSimulation(1.00f, 0);
    }

    // 3. Drena e transmite eventos consolidados
    if (_eventQueue != nullptr) {
        PowerQualityEvent evt;
        while (xQueueReceive(_eventQueue, &evt, 0) == pdTRUE) {
            sendEvent(evt);
        }
    }

    // 4. Drena e transmite telemetria (~10 Hz)
    if (_telemetryQueue != nullptr) {
        TelemetryData telem;
        if (xQueueReceive(_telemetryQueue, &telem, 0) == pdTRUE) {
            sendTelemetry(telem);
        }
    }
}
