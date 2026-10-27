# Especificação do Protocolo de Comunicação (ESP32 <-> Servidor Python :3000)

Este documento descreve os padrões de mensagens JSON trocadas via WebSocket entre as sondas ESP32 e o Servidor Central Python.

---

## 1. Conexão e Handshake

As sondas ESP32 conectam-se ao servidor através do endpoint:
```
ws://<IP_DO_SERVIDOR>:3000/ws/device?device_id=<ID_DA_SONDA>
```

- **Query Param `device_id`:** String alfanumérica única identificando a sonda (ex: `ESP32-SUBESTACAO-01`, `ESP32-TRAFO-02`).
- Ao estabelecer a conexão, o servidor registra o nó em memória e transmite a lista atualizada para os dashboards web conectados em `/ws/ui`.

---

## 2. Mensagens da Sonda ESP32 para o Servidor

### 2.1 Stream Contínuo de Telemetria (~10 Hz)
Transmitido periodicamente pelo Core 0 (decimação a cada 6 ciclos de 60 Hz = ~100 ms):

```json
{
  "device_id": "ESP32-SUBESTACAO-01",
  "type": "telemetry",
  "timestamp_ms": 1719502000,
  "v_rms": 127.12,
  "pu": 1.001,
  "freq": 60.0,
  "state": "NORMAL",
  "cycle": 14200,
  "active_dur": 0,
  "active_extreme": 1.001
}
```

- `state`: `"NORMAL"`, `"SAG"` ou `"SWELL"`
- `active_dur`: Duração em milissegundos do evento em andamento (0 se NORMAL).
- `active_extreme`: Menor pu registrado se SAG, ou maior pu registrado se SWELL.

### 2.2 Evento Consolidado IEC 61000-4-30 (Fim de Distúrbio)
Transmitido imediatamente no término de um afundamento ou elevação após verificação da histerese:

```json
{
  "device_id": "ESP32-SUBESTACAO-01",
  "type": "event",
  "id": 3,
  "class": "SAG",
  "timestamp_ms": 1719501500,
  "duration_ms": 380,
  "duration_cycles": 23,
  "extreme_voltage": 85.4,
  "extreme_pu": 0.672
}
```

- `class`: `"SAG"` (Afundamento) ou `"SWELL"` (Elevação).
- `duration_cycles`: Número de ciclos completos de 60 Hz em que o distúrbio permaneceu ativo.

---

## 3. Mensagens do Servidor para a Sonda ESP32

### 3.1 Atualização Dinâmica de Parâmetros
Permite alterar limiares sem regravar o microcontrolador:

```json
{
  "type": "config",
  "v_ref": 127.0,
  "sag_threshold": 0.88,
  "swell_threshold": 1.12,
  "hysteresis": 0.02
}
```

### 3.2 Injeção de Cenários Didáticos / Simulação
Comanda o gerador interno do Core 1 a sintetizar formas de onda de teste:

```json
{
  "type": "simulate",
  "scenario": "sag",
  "pu": 0.75,
  "duration_ms": 400
}
```
*(Para retornar à leitura do pino analógico físico GPIO 34, envia-se `{"type": "simulate", "scenario": "adc"}`).*

---

## 4. Endpoints REST do Servidor Python

| Método | Endpoint | Descrição |
|---|---|---|
| `GET` | `/` | Retorna o dashboard web responsivo |
| `GET` | `/api/devices` | Lista todas as sondas conhecidas e status online/offline |
| `GET` | `/api/devices/{id}/config` | Retorna a configuração atual da sonda |
| `POST` | `/api/devices/{id}/config` | Envia novos limiares para a sonda selecionada |
| `POST` | `/api/devices/{id}/simulate` | Dispara simulação remota na sonda selecionada |
| `GET` | `/api/devices/{id}/events` | Retorna histórico de eventos da sonda |
| `GET` | `/api/events` | Retorna histórico global de todos os eventos consolidados |
| `POST` | `/api/simulate-virtual-probe` | Inicia sonda virtual simulada para demonstração |
| `WS` | `/ws/ui` | Canal de atualização em tempo real para os navegadores |
