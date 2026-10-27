"""
Simulador de Nó ESP32 para Testes de Bancada e Validação do Servidor Python
Conecta em ws://localhost:3000/ws/device e transmite telemetria IEC 61000-4-30 a 10 Hz.
"""

import asyncio
import json
import random
import time
import argparse
import websockets

async def run_simulator(server_url: str, device_id: str, v_ref: float):
    print(f"[*] Iniciando simulação do ESP32: '{device_id}'")
    print(f"[*] Conectando ao servidor: {server_url}/ws/device?device_id={device_id}")

    cycle = 0
    event_id = 0
    sim_pu = 1.0
    sim_end_time = 0.0
    state = "NORMAL"
    event_start_time = 0
    extreme_pu = 1.0

    sag_th = 0.90
    swell_th = 1.10
    hyst = 0.02

    async for ws in websockets.connect(f"{server_url}/ws/device?device_id={device_id}"):
        try:
            print(f"[+] Conectado com sucesso ao servidor na porta 3000 como {device_id}!")
            
            async def receive_commands():
                nonlocal sim_pu, sim_end_time, sag_th, swell_th, hyst, v_ref
                async for msg in ws:
                    try:
                        data = json.loads(msg)
                        mtype = data.get("type")
                        if mtype == "simulate":
                            scen = data.get("scenario", "nominal")
                            dur = data.get("duration_ms", 0) / 1000.0
                            pu_target = data.get("pu", 1.0)
                            print(f"[CMD] Simulação recebida: {scen} -> {pu_target} pu por {dur}s")
                            sim_pu = pu_target
                            if dur > 0:
                                sim_end_time = time.time() + dur
                            else:
                                sim_end_time = 0.0
                        elif mtype == "config":
                            print(f"[CMD] Configuração recebida: {data}")
                            if "v_ref" in data: v_ref = data["v_ref"]
                            if "sag_threshold" in data: sag_th = data["sag_threshold"]
                            if "swell_threshold" in data: swell_th = data["swell_threshold"]
                            if "hysteresis" in data: hyst = data["hysteresis"]
                    except Exception as e:
                        print(f"Erro ao processar comando: {e}")

            rx_task = asyncio.create_task(receive_commands())

            while True:
                await asyncio.sleep(0.1) # 10 Hz
                cycle += 6
                now_ms = int(time.time() * 1000)

                if sim_end_time > 0 and time.time() >= sim_end_time:
                    sim_pu = 1.0
                    sim_end_time = 0.0

                # Pequeno ruído elétrico realista de medição
                noise = random.uniform(-0.003, 0.003)
                pu = sim_pu + noise
                v_rms = pu * v_ref

                # Máquina de estados IEC 61000-4-30
                if state == "NORMAL":
                    if pu <= sag_th:
                        state = "SAG"
                        event_start_time = now_ms
                        extreme_pu = pu
                        event_id += 1
                        print(f"[!] SAG INICIADO em {device_id}: {pu:.3f} pu")
                    elif pu >= swell_th:
                        state = "SWELL"
                        event_start_time = now_ms
                        extreme_pu = pu
                        event_id += 1
                        print(f"[!] SWELL INICIADO em {device_id}: {pu:.3f} pu")
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
                        await ws.send(json.dumps(evt))
                        print(f"[✔] SAG FINALIZADO em {device_id}: {dur_ms} ms, {extreme_pu:.3f} pu")
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
                        await ws.send(json.dumps(evt))
                        print(f"[✔] SWELL FINALIZADO em {device_id}: {dur_ms} ms, {extreme_pu:.3f} pu")
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
                await ws.send(json.dumps(telem))

        except websockets.ConnectionClosed:
            print("[-] Conexão perdida. Tentando reconectar em 2s...")
            await asyncio.sleep(2)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Simulador de Nó ESP32 para o Servidor Metrum PQ")
    parser.add_argument("--server", default="ws://localhost:3000", help="URL do Servidor (ex: ws://192.168.1.100:3000)")
    parser.add_argument("--device", default="ESP32-SUBESTACAO-01", help="ID do dispositivo (ex: ESP32-PQ-01)")
    parser.add_argument("--vref", type=float, default=127.0, help="Tensão de referência RMS")
    args = parser.parse_args()

    asyncio.run(run_simulator(args.server, args.device, args.vref))
