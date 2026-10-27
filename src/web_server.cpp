#include "web_server.h"
#include "html_content.h"

WebServerManager::WebServerManager()
    : _server(80),
      _ws("/ws"),
      _telemetryQueue(nullptr),
      _eventQueue(nullptr),
      _configMutex(nullptr),
      _config(nullptr),
      _eventCount(0),
      _eventHead(0),
      _simTimerActive(false),
      _simEndTimeMs(0)
{
    memset(_eventHistory, 0, sizeof(_eventHistory));
}

void WebServerManager::begin(QueueHandle_t telemetryQ, QueueHandle_t eventQ, SemaphoreHandle_t cfgMutex, PowerQualityConfig* cfgPtr) {
    _telemetryQueue = telemetryQ;
    _eventQueue = eventQ;
    _configMutex = cfgMutex;
    _config = cfgPtr;

    // Inicializa sistema de arquivos LittleFS para assets estáticos
    if (!LittleFS.begin(true)) {
        Serial.println("[LittleFS] Atenção: Sistema de arquivos não pôde ser montado. Usando PROGMEM fallback.");
    } else {
        Serial.println("[LittleFS] Montado com sucesso.");
    }

    // Configura eventos do WebSocket
    _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWsEvent(server, client, type, arg, data, len);
    });
    _server.addHandler(&_ws);

    // Configura Rotas HTTP / REST API
    setupRoutes();

    // Inicia Servidor Web
    _server.begin();
    Serial.println("[HTTP] Servidor Web Assíncrono e WebSocket iniciados na porta 80.");
}

void WebServerManager::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Cliente #%u conectado de %s\n", client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Cliente #%u desconectado\n", client->id());
    } else if (type == WS_EVT_ERROR) {
        Serial.printf("[WS] Erro no cliente #%u\n", client->id());
    }
}

void WebServerManager::setupRoutes() {
    // 1. Rota Principal (Dashboard HTML5)
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            // Fallback seguro direto da Flash (PROGMEM)
            request->send_P(200, "text/html", INDEX_HTML);
        }
    });

    // 2. REST API: GET /api/config
    _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            doc["v_ref"]           = _config->v_ref;
            doc["sag_threshold"]   = _config->sag_threshold;
            doc["swell_threshold"] = _config->swell_threshold;
            doc["hysteresis"]      = _config->hysteresis;
            doc["adc_calibration"] = _config->adc_calibration;
            doc["simulation_mode"] = _config->simulation_mode;
            doc["sim_voltage_pu"]  = _config->sim_voltage_pu;
            xSemaphoreGive(_configMutex);
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // 3. REST API: POST /api/config (Atualização dinâmica sem reiniciar ESP32)
    _server.on("/api/config", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            // Resposta após processamento do corpo
        },
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (doc["v_ref"].is<float>())           _config->v_ref = doc["v_ref"].as<float>();
                if (doc["sag_threshold"].is<float>())   _config->sag_threshold = doc["sag_threshold"].as<float>();
                if (doc["swell_threshold"].is<float>()) _config->swell_threshold = doc["swell_threshold"].as<float>();
                if (doc["hysteresis"].is<float>())      _config->hysteresis = doc["hysteresis"].as<float>();
                if (doc["adc_calibration"].is<float>()) _config->adc_calibration = doc["adc_calibration"].as<float>();
                xSemaphoreGive(_configMutex);
            }

            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuração atualizada com sucesso\"}");
        }
    );

    // 4. REST API: GET /api/events (Histórico dos últimos eventos)
    _server.on("/api/events", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        int count = _eventCount;
        int idx = _eventHead;
        for (int i = 0; i < count; i++) {
            idx = (idx - 1 + MAX_HISTORICAL_EVENTS) % MAX_HISTORICAL_EVENTS;
            const PowerQualityEvent& e = _eventHistory[idx];

            JsonObject item = arr.add<JsonObject>();
            item["id"]              = e.id;
            item["class"]           = (e.type == STATE_SAG) ? "SAG" : "SWELL";
            item["timestamp_ms"]    = e.timestamp_ms;
            item["duration_ms"]     = e.duration_ms;
            item["duration_cycles"] = e.duration_cycles;
            item["extreme_voltage"] = e.extreme_voltage;
            item["extreme_pu"]      = e.extreme_pu;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // 5. REST API: POST /api/events/clear
    _server.on("/api/events/clear", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _eventCount = 0;
        _eventHead = 0;
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Histórico limpo\"}");
    });

    // 6. REST API: POST /api/simulate (Bancada Didática / Demonstração HackatOhm)
    _server.on("/api/simulate", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            deserializeJson(doc, data, len);

            String scenario = doc["scenario"] | "nominal";
            float pu = doc["pu"] | 1.0f;
            uint32_t duration = doc["duration_ms"] | 0;

            if (scenario == "adc") {
                this->stopSimulation();
            } else {
                this->triggerSimulation(pu, duration);
            }

            request->send(200, "application/json", "{\"status\":\"ok\",\"scenario\":\"" + scenario + "\"}");
        }
    );
}

void WebServerManager::triggerSimulation(float pu, uint32_t durationMs) {
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

void WebServerManager::stopSimulation() {
    if (xSemaphoreTake(_configMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        _config->simulation_mode = false;
        _config->sim_voltage_pu = 1.0f;
        xSemaphoreGive(_configMutex);
    }
    _simTimerActive = false;
}

void WebServerManager::addEventToHistory(const PowerQualityEvent& evt) {
    _eventHistory[_eventHead] = evt;
    _eventHead = (_eventHead + 1) % MAX_HISTORICAL_EVENTS;
    if (_eventCount < MAX_HISTORICAL_EVENTS) {
        _eventCount++;
    }
}

void WebServerManager::broadcastTelemetry(const TelemetryData& telem) {
    if (_ws.count() == 0) return; // Economiza CPU se não houver clientes conectados

    JsonDocument doc;
    doc["type"]           = "telemetry";
    doc["timestamp"]      = telem.timestamp_ms;
    doc["v_rms"]          = telem.v_rms;
    doc["pu"]             = telem.pu;
    doc["freq"]           = telem.frequency;
    doc["state"]          = (telem.state == STATE_SAG) ? "SAG" : ((telem.state == STATE_SWELL) ? "SWELL" : "NORMAL");
    doc["cycle"]          = telem.current_cycle;
    doc["active_dur"]     = telem.active_event_duration_ms;
    doc["active_extreme"] = telem.active_event_extreme_pu;

    String json;
    serializeJson(doc, json);
    _ws.textAll(json);
}

void WebServerManager::broadcastEvent(const PowerQualityEvent& evt) {
    if (_ws.count() == 0) return;

    JsonDocument doc;
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
    _ws.textAll(json);
}

void WebServerManager::update() {
    // 1. Limpeza de conexões inativas WebSocket
    _ws.cleanupClients();

    // 2. Temporizador para simulação com auto-reversão para nominal
    if (_simTimerActive && millis() >= _simEndTimeMs) {
        // Distúrbio temporizado encerrou, retorna a 1.0 pu
        triggerSimulation(1.00f, 0);
    }

    // 3. Drena eventos consolidados da Fila do Core 1
    if (_eventQueue != nullptr) {
        PowerQualityEvent evt;
        while (xQueueReceive(_eventQueue, &evt, 0) == pdTRUE) {
            addEventToHistory(evt);
            broadcastEvent(evt);
            Serial.printf("[PQ-EVENT] #%u %s | Dur: %u ms (%u ciclos) | Extremo: %.1f V (%.3f pu)\n",
                evt.id,
                (evt.type == STATE_SAG) ? "SAG" : "SWELL",
                evt.duration_ms,
                evt.duration_cycles,
                evt.extreme_voltage,
                evt.extreme_pu
            );
        }
    }

    // 4. Drena pacote de telemetria mais recente (~10 Hz)
    if (_telemetryQueue != nullptr) {
        TelemetryData telem;
        if (xQueueReceive(_telemetryQueue, &telem, 0) == pdTRUE) {
            broadcastTelemetry(telem);
        }
    }
}
