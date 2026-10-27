"""
Servidor Central de Monitoramento de Qualidade de Energia (IEC 61000-4-30)
Desafio Metrum / HackatOhm
Executa na porta 3000 e suporta conexões de múltiplos ESP32s simultâneos via WebSocket.
"""

import asyncio
import json
import os
import time
from typing import Dict, List, Optional, Set
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Query, HTTPException
from fastapi.responses import HTMLResponse, JSONResponse
from pydantic import BaseModel

app = FastAPI(
    title="Metrum Power Quality Server",
    description="Servidor Central de Ingestão e Monitoramento de Distúrbios IEC 61000-4-30 (Sag/Swell)",
    version="2.0.0"
)

# Caminho dos arquivos estáticos
STATIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")

# Estrutura em memória dos dispositivos e conexões
class DeviceState:
    def __init__(self, device_id: str, client_ip: str = "127.0.0.1"):
        self.device_id = device_id
        self.client_ip = client_ip
        self.ws: Optional[WebSocket] = None
        self.connected_at = time.time()
        self.last_seen = time.time()
        self.config = {
            "v_ref": 127.0,
            "sag_threshold": 0.90,
            "swell_threshold": 1.10,
            "hysteresis": 0.02,
            "adc_calibration": 0.125
        }
        self.latest_telemetry: Optional[dict] = None
        self.events: List[dict] = []
        self.is_virtual = False

devices: Dict[str, DeviceState] = {}
ui_clients: Set[WebSocket] = set()

# Modelos Pydantic para validação REST
class ConfigModel(BaseModel):
    v_ref: float = 127.0
    sag_threshold: float = 0.90
    swell_threshold: float = 1.10
    hysteresis: float = 0.02
    adc_calibration: Optional[float] = 0.125

class SimulateModel(BaseModel):
    scenario: str = "nominal"
    pu: float = 1.0
    duration_ms: int = 0

async def broadcast_to_ui(payload: dict):
    """Envia payload JSON para todos os navegadores/dashboards conectados."""
    if not ui_clients:
        return
    message = json.dumps(payload)
    disconnected = set()
    for ws in ui_clients:
        try:
            await ws.send_text(message)
        except Exception:
            disconnected.add(ws)
    for ws in disconnected:
        ui_clients.discard(ws)

def get_devices_summary() -> List[dict]:
    """Retorna lista de dispositivos cadastrados/conectados com status de conexão."""
    now = time.time()
    summary = []
    for dev_id, state in devices.items():
        online = (state.ws is not None) or (now - state.last_seen < 3.0)
        summary.append({
            "device_id": dev_id,
            "client_ip": state.client_ip,
            "online": online,
            "last_seen": int(state.last_seen),
            "is_virtual": state.is_virtual,
            "config": state.config
        })
    return summary

# ==========================================================
# Rota Principal: Dashboard Web
# ==========================================================
@app.get("/", response_class=HTMLResponse)
async def get_index():
    index_file = os.path.join(STATIC_DIR, "index.html")
    if os.path.exists(index_file):
        with open(index_file, "r", encoding="utf-8") as f:
            return HTMLResponse(content=f.read())
    return HTMLResponse(content="<h1>Metrum PQ Server Online na porta 3000</h1><p>index.html não encontrado.</p>")

# ==========================================================
# WebSocket para Sondas ESP32 (/ws/device)
# ==========================================================
@app.websocket("/ws/device")
async def websocket_device_endpoint(
    websocket: WebSocket,
    device_id: str = Query("ESP32-DEFAULT")
):
    await websocket.accept()
    client_ip = websocket.client.host if websocket.client else "unknown"

    if device_id not in devices:
        devices[device_id] = DeviceState(device_id, client_ip)
    
    dev_state = devices[device_id]
    dev_state.ws = websocket
    dev_state.client_ip = client_ip
    dev_state.last_seen = time.time()
    dev_state.is_virtual = False

    print(f"[CONEXÃO] ESP32 conectado: ID='{device_id}' | IP={client_ip}")

    # Notifica os dashboards sobre a nova sonda
    await broadcast_to_ui({
        "type": "device_list",
        "devices": get_devices_summary()
    })

    try:
        while True:
            text = await websocket.receive_text()
            dev_state.last_seen = time.time()
            try:
                data = json.loads(text)
            except json.JSONDecodeError:
                continue

            msg_type = data.get("type")
            data["device_id"] = device_id

            if msg_type == "telemetry":
                dev_state.latest_telemetry = data
                await broadcast_to_ui(data)

            elif msg_type == "event":
                dev_state.events.insert(0, data)
                if len(dev_state.events) > 200:
                    dev_state.events.pop()
                print(f"[EVENTO IEC] {device_id} -> {data.get('class')} #{data.get('id')} ({data.get('duration_ms')} ms, {data.get('extreme_pu')} pu)")
                await broadcast_to_ui(data)

            elif msg_type == "config_ack":
                dev_state.config.update(data.get("config", {}))

    except WebSocketDisconnect:
        print(f"[DESCONEXÃO] ESP32 desconectado: ID='{device_id}'")
        dev_state.ws = None
        await broadcast_to_ui({
            "type": "device_list",
            "devices": get_devices_summary()
        })
    except Exception as e:
        print(f"[ERRO WS] ESP32 {device_id}: {e}")
        dev_state.ws = None

# ==========================================================
# WebSocket para Navegadores / Dashboard UI (/ws/ui)
# ==========================================================
@app.websocket("/ws/ui")
async def websocket_ui_endpoint(websocket: WebSocket):
    await websocket.accept()
    ui_clients.add(websocket)

    # Envia lista de dispositivos e histórico atual
    await websocket.send_text(json.dumps({
        "type": "device_list",
        "devices": get_devices_summary()
    }))

    try:
        while True:
            # Mantém conexão viva aguardando pings ou mensagens da UI
            await websocket.receive_text()
    except WebSocketDisconnect:
        ui_clients.discard(websocket)
    except Exception:
        ui_clients.discard(websocket)

# ==========================================================
# Endpoints REST para Gerenciamento e Configuração
# ==========================================================
@app.get("/api/devices")
async def api_get_devices():
    return get_devices_summary()

@app.get("/api/devices/{device_id}/config")
async def api_get_device_config(device_id: str):
    if device_id not in devices:
        raise HTTPException(status_code=404, detail="Dispositivo não encontrado")
    return devices[device_id].config

@app.post("/api/devices/{device_id}/config")
async def api_set_device_config(device_id: str, cfg: ConfigModel):
    if device_id not in devices:
        raise HTTPException(status_code=404, detail="Dispositivo não encontrado")
    
    dev = devices[device_id]
    new_cfg = cfg.model_dump()
    dev.config.update(new_cfg)

    # Se a sonda física estiver conectada via WebSocket, despacha comando em tempo real
    if dev.ws:
        payload = {"type": "config", **new_cfg}
        try:
            await dev.ws.send_text(json.dumps(payload))
        except Exception as e:
            print(f"[ERRO ENVIO CONFIG] {e}")

    return {"status": "ok", "message": f"Configuração enviada para {device_id}", "config": dev.config}

@app.post("/api/devices/{device_id}/simulate")
async def api_simulate_on_device(device_id: str, sim: SimulateModel):
    if device_id not in devices:
        raise HTTPException(status_code=404, detail="Dispositivo não encontrado")
    
    dev = devices[device_id]
    payload = {
        "type": "simulate",
        "scenario": sim.scenario,
        "pu": sim.pu,
        "duration_ms": sim.duration_ms
    }

    if dev.ws:
        try:
            await dev.ws.send_text(json.dumps(payload))
        except Exception as e:
            print(f"[ERRO ENVIO SIMULATE] {e}")

    return {"status": "ok", "scenario": sim.scenario}

@app.get("/api/devices/{device_id}/events")
async def api_get_device_events(device_id: str):
    if device_id not in devices:
        raise HTTPException(status_code=404, detail="Dispositivo não encontrado")
    return devices[device_id].events

@app.get("/api/events")
async def api_get_all_events():
    all_events = []
    for dev in devices.values():
        all_events.extend(dev.events)
    # Ordena decrescente por timestamp
    all_events.sort(key=lambda x: x.get("timestamp_ms", 0), reverse=True)
    return all_events

# ==========================================================
# Injetor de Nó Virtual (Para Testes e Apresentações sem ESP32 Físico)
# ==========================================================
virtual_tasks: Dict[str, asyncio.Task] = {}

async def run_virtual_probe(device_id: str):
    """Simula um ESP32 gerando 60 Hz e detecção IEC 61000-4-30 a 10 Hz."""
    print(f"[VIRTUAL] Sonda virtual iniciada: {device_id}")
    dev_state = devices[device_id]
    dev_state.is_virtual = True

    cycle = 0
    event_id = 0
    sim_pu = 1.0
    sim_end_time = 0.0
    state = "NORMAL"
    event_start_time = 0
    extreme_pu = 1.0

    while True:
        cycle += 6  # 6 ciclos por pacote a 10 Hz
        now_ms = int(time.time() * 1000)

        # Checa retorno de simulação
        if sim_end_time > 0 and time.time() >= sim_end_time:
            sim_pu = 1.0
            sim_end_time = 0.0

        v_ref = dev_state.config.get("v_ref", 127.0)
        v_rms = sim_pu * v_ref
        pu = sim_pu

        # Máquina de Estados IEC 61000-4-30
        sag_th = dev_state.config.get("sag_threshold", 0.90)
        swell_th = dev_state.config.get("swell_threshold", 1.10)
        hyst = dev_state.config.get("hysteresis", 0.02)

        if state == "NORMAL":
            if pu <= sag_th:
                state = "SAG"
                event_start_time = now_ms
                extreme_pu = pu
                event_id += 1
            elif pu >= swell_th:
                state = "SWELL"
                event_start_time = now_ms
                extreme_pu = pu
                event_id += 1
        elif state == "SAG":
            extreme_pu = min(extreme_pu, pu)
            if pu >= (sag_th + hyst):
                dur_ms = now_ms - event_start_time
                evt = {
                    "device_id": device_id,
                    "type": "event",
                    "id": event_id,
                    "class": "SAG",
                    "timestamp_ms": event_start_time,
                    "duration_ms": dur_ms,
                    "duration_cycles": int(dur_ms / 16.666),
                    "extreme_voltage": extreme_pu * v_ref,
                    "extreme_pu": extreme_pu
                }
                dev_state.events.insert(0, evt)
                await broadcast_to_ui(evt)
                state = "NORMAL"
        elif state == "SWELL":
            extreme_pu = max(extreme_pu, pu)
            if pu <= (swell_th - hyst):
                dur_ms = now_ms - event_start_time
                evt = {
                    "device_id": device_id,
                    "type": "event",
                    "id": event_id,
                    "class": "SWELL",
                    "timestamp_ms": event_start_time,
                    "duration_ms": dur_ms,
                    "duration_cycles": int(dur_ms / 16.666),
                    "extreme_voltage": extreme_pu * v_ref,
                    "extreme_pu": extreme_pu
                }
                dev_state.events.insert(0, evt)
                await broadcast_to_ui(evt)
                state = "NORMAL"

        telem = {
            "device_id": device_id,
            "type": "telemetry",
            "timestamp_ms": now_ms,
            "v_rms": v_rms,
            "pu": pu,
            "freq": 60.0,
            "state": state,
            "cycle": cycle,
            "active_dur": (now_ms - event_start_time) if state != "NORMAL" else 0,
            "active_extreme": extreme_pu if state != "NORMAL" else pu
        }
        dev_state.latest_telemetry = telem
        dev_state.last_seen = time.time()
        await broadcast_to_ui(telem)

        await asyncio.sleep(0.1)

@app.post("/api/simulate-virtual-probe")
async def api_start_virtual_probe(device_id: str = "ESP32-VIRTUAL-01"):
    if device_id not in devices:
        devices[device_id] = DeviceState(device_id, "127.0.0.1")
    
    if device_id not in virtual_tasks or virtual_tasks[device_id].done():
        task = asyncio.create_task(run_virtual_probe(device_id))
        virtual_tasks[device_id] = task
        await broadcast_to_ui({
            "type": "device_list",
            "devices": get_devices_summary()
        })
        return {"status": "ok", "message": f"Sonda virtual '{device_id}' iniciada transmitindo a 10 Hz!"}
    return {"status": "already_running", "message": f"Sonda '{device_id}' já está em execução."}

if __name__ == "__main__":
    import uvicorn
    print("\n==================================================================")
    print("  METRUM PQ SERVER - Monitor Multi-ESP32 (IEC 61000-4-30)")
    print("  Escutando na porta 3000 (todas as interfaces: 0.0.0.0:3000)")
    print("  Acesse o Dashboard em: http://localhost:3000")
    print("  WebSocket de Dispositivos: ws://<IP>:3000/ws/device?device_id=...")
    print("==================================================================\n")
    uvicorn.run(app, host="0.0.0.0", port=3000, log_level="info")
