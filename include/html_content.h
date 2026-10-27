#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

#include <pgmspace.h>

// Fallback HTML embutido em memória Flash (PROGMEM)
// Permite que o servidor web funcione mesmo que o usuário não execute "Upload LittleFS"
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=5.0, user-scalable=yes">
  <title>Metrum PQ | Monitor IEC 61000-4-30 (Sag & Swell)</title>
  <style>
    :root {
      --bg-dark: #0b0f19;
      --card-bg: #111827;
      --card-border: #1f293d;
      --card-hover: #1a2336;
      --text-main: #f3f4f6;
      --text-muted: #9ca3af;
      --primary: #38bdf8;
      --primary-glow: rgba(56, 189, 248, 0.25);
      --success: #10b981;
      --success-glow: rgba(16, 185, 129, 0.25);
      --warning: #f59e0b;
      --warning-glow: rgba(245, 158, 11, 0.25);
      --danger: #ef4444;
      --danger-glow: rgba(239, 68, 68, 0.25);
      --accent: #6366f1;
    }
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      -webkit-tap-highlight-color: transparent;
    }
    body {
      background-color: var(--bg-dark);
      color: var(--text-main);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
      min-height: 100vh;
      overflow-x: hidden;
      display: flex;
      flex-direction: column;
    }
    .container {
      width: 100%;
      max-width: 1200px;
      margin: 0 auto;
      padding: 0.75rem;
      flex: 1;
    }
    @media (min-width: 640px) { .container { padding: 1.25rem; } }
    @media (min-width: 1024px) { .container { padding: 1.75rem; } }

    header {
      background-color: rgba(17, 24, 39, 0.85);
      backdrop-filter: blur(12px);
      border-bottom: 1px solid var(--card-border);
      position: sticky;
      top: 0;
      z-index: 50;
    }
    .header-content {
      display: flex;
      flex-direction: column;
      gap: 0.75rem;
      padding: 0.85rem 0.75rem;
      max-width: 1200px;
      margin: 0 auto;
    }
    @media (min-width: 640px) {
      .header-content {
        flex-direction: row;
        align-items: center;
        justify-content: space-between;
        padding: 0.85rem 1.25rem;
      }
    }
    .brand { display: flex; align-items: center; gap: 0.75rem; }
    .logo-badge {
      background: linear-gradient(135deg, #0284c7, #6366f1);
      width: 44px;
      height: 44px;
      border-radius: 10px;
      display: flex;
      align-items: center;
      justify-content: center;
      font-weight: 900;
      font-size: 1.2rem;
      color: #fff;
      box-shadow: 0 4px 12px rgba(2, 132, 199, 0.35);
      flex-shrink: 0;
    }
    .brand-text h1 { font-size: 1.05rem; font-weight: 700; color: #fff; line-height: 1.2; }
    .brand-text p { font-size: 0.72rem; color: var(--text-muted); text-transform: uppercase; }
    .header-status { display: flex; align-items: center; gap: 0.5rem; flex-wrap: wrap; }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 0.4rem;
      padding: 0.35rem 0.65rem;
      border-radius: 9999px;
      font-size: 0.75rem;
      font-weight: 600;
      background: rgba(31, 41, 61, 0.7);
      border: 1px solid var(--card-border);
      min-height: 32px;
    }
    .badge-dot { width: 8px; height: 8px; border-radius: 50%; background-color: var(--text-muted); }
    .badge-dot.connected { background-color: var(--success); box-shadow: 0 0 8px var(--success); animation: pulse 2s infinite; }
    .badge-dot.disconnected { background-color: var(--danger); box-shadow: 0 0 8px var(--danger); }
    @keyframes pulse { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.5; transform: scale(0.85); } }

    .metrics-grid {
      display: grid;
      grid-template-columns: 1fr;
      gap: 0.85rem;
      margin-top: 1rem;
    }
    @media (min-width: 480px) { .metrics-grid { grid-template-columns: repeat(2, 1fr); } }
    @media (min-width: 1024px) { .metrics-grid { grid-template-columns: repeat(4, 1fr); } }

    .card {
      background-color: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 1rem;
    }
    .card-title {
      font-size: 0.78rem;
      text-transform: uppercase;
      color: var(--text-muted);
      margin-bottom: 0.4rem;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .card-value { font-size: 1.85rem; font-weight: 800; line-height: 1.1; }
    .card-unit { font-size: 0.95rem; font-weight: 500; color: var(--text-muted); margin-left: 0.25rem; }
    .card-subtext { font-size: 0.75rem; color: var(--text-muted); margin-top: 0.4rem; }

    .status-card.state-normal {
      border-color: rgba(16, 185, 129, 0.4);
      background: linear-gradient(180deg, rgba(16, 185, 129, 0.08) 0%, var(--card-bg) 100%);
    }
    .status-card.state-normal .status-indicator { color: var(--success); }
    .status-card.state-sag {
      border-color: rgba(245, 158, 11, 0.7);
      background: linear-gradient(180deg, rgba(245, 158, 11, 0.15) 0%, var(--card-bg) 100%);
      animation: alertPulse 1.2s infinite alternate;
    }
    .status-card.state-sag .status-indicator { color: var(--warning); }
    .status-card.state-swell {
      border-color: rgba(239, 68, 68, 0.8);
      background: linear-gradient(180deg, rgba(239, 68, 68, 0.18) 0%, var(--card-bg) 100%);
      animation: alertPulse 0.9s infinite alternate;
    }
    .status-card.state-swell .status-indicator { color: var(--danger); }
    @keyframes alertPulse { from { border-color: rgba(245, 158, 11, 0.4); } to { border-color: rgba(245, 158, 11, 1); } }

    .chart-section { margin-top: 1.25rem; }
    .chart-container {
      background-color: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 1rem;
      display: flex;
      flex-direction: column;
      gap: 0.75rem;
    }
    .chart-header { display: flex; flex-direction: column; gap: 0.5rem; }
    @media (min-width: 640px) {
      .chart-header { flex-direction: row; justify-content: space-between; align-items: center; }
    }
    .chart-title { font-size: 0.95rem; font-weight: 700; color: #fff; }
    .chart-legend { display: flex; gap: 0.75rem; flex-wrap: wrap; font-size: 0.75rem; }
    .legend-item { display: inline-flex; align-items: center; gap: 0.35rem; }
    .legend-line { width: 14px; height: 3px; border-radius: 2px; }
    .canvas-wrap { position: relative; width: 100%; height: 220px; }
    @media (min-width: 768px) { .canvas-wrap { height: 280px; } }
    canvas { width: 100% !important; height: 100% !important; display: block; border-radius: 6px; }

    .test-bench {
      margin-top: 1.25rem;
      background: linear-gradient(180deg, #161e2e 0%, var(--card-bg) 100%);
      border: 1px solid #2d3748;
      border-radius: 12px;
      padding: 1.1rem;
    }
    .bench-title {
      font-size: 0.85rem;
      font-weight: 700;
      text-transform: uppercase;
      color: var(--primary);
      margin-bottom: 0.25rem;
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }
    .bench-desc { font-size: 0.75rem; color: var(--text-muted); margin-bottom: 0.85rem; }
    .button-group { display: grid; grid-template-columns: 1fr; gap: 0.6rem; }
    @media (min-width: 480px) { .button-group { grid-template-columns: repeat(2, 1fr); } }
    @media (min-width: 768px) { .button-group { grid-template-columns: repeat(4, 1fr); } }

    .btn {
      min-height: 44px;
      min-width: 44px;
      padding: 0.7rem 1rem;
      border-radius: 8px;
      font-size: 0.82rem;
      font-weight: 600;
      cursor: pointer;
      border: 1px solid transparent;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 0.45rem;
      transition: all 0.2s ease;
      touch-action: manipulation;
    }
    .btn:active { transform: scale(0.98); }
    .btn-nominal { background-color: rgba(16, 185, 129, 0.15); border-color: rgba(16, 185, 129, 0.35); color: #34d399; }
    .btn-sag { background-color: rgba(245, 158, 11, 0.15); border-color: rgba(245, 158, 11, 0.35); color: #fbbf24; }
    .btn-swell { background-color: rgba(239, 68, 68, 0.15); border-color: rgba(239, 68, 68, 0.35); color: #f87171; }
    .btn-hardware { background-color: rgba(56, 189, 248, 0.15); border-color: rgba(56, 189, 248, 0.35); color: #7dd3fc; }
    .btn-secondary { background-color: #1f2937; border-color: var(--card-border); color: var(--text-main); }
    .btn-primary { background: linear-gradient(135deg, #0284c7, #2563eb); color: #fff; }

    .history-section { margin-top: 1.25rem; margin-bottom: 2rem; }
    .history-card { background-color: var(--card-bg); border: 1px solid var(--card-border); border-radius: 12px; padding: 1rem; }
    .history-header { display: flex; flex-direction: column; gap: 0.75rem; margin-bottom: 1rem; }
    @media (min-width: 640px) {
      .history-header { flex-direction: row; justify-content: space-between; align-items: center; }
    }
    .history-actions { display: flex; gap: 0.5rem; flex-wrap: wrap; }
    .table-responsive { width: 100%; overflow-x: auto; border-radius: 8px; border: 1px solid var(--card-border); }
    table { width: 100%; border-collapse: collapse; text-align: left; font-size: 0.8rem; min-width: 600px; }
    th {
      background-color: #0f172a;
      color: var(--text-muted);
      font-weight: 600;
      padding: 0.75rem 0.85rem;
      border-bottom: 1px solid var(--card-border);
      font-size: 0.7rem;
      text-transform: uppercase;
    }
    td { padding: 0.75rem 0.85rem; border-bottom: 1px solid rgba(31, 41, 61, 0.6); white-space: nowrap; }
    .badge-event { display: inline-flex; align-items: center; padding: 0.25rem 0.55rem; border-radius: 4px; font-weight: 700; font-size: 0.7rem; }
    .badge-sag { background-color: rgba(245, 158, 11, 0.2); color: #fbbf24; border: 1px solid rgba(245, 158, 11, 0.4); }
    .badge-swell { background-color: rgba(239, 68, 68, 0.2); color: #f87171; border: 1px solid rgba(239, 68, 68, 0.4); }

    .config-panel { background-color: var(--card-bg); border: 1px solid var(--card-border); border-radius: 12px; padding: 1.1rem; }
    .form-grid { display: grid; grid-template-columns: 1fr; gap: 0.85rem; margin-top: 0.85rem; }
    @media (min-width: 640px) { .form-grid { grid-template-columns: repeat(2, 1fr); } }
    @media (min-width: 1024px) { .form-grid { grid-template-columns: repeat(4, 1fr); } }
    .form-group { display: flex; flex-direction: column; gap: 0.35rem; }
    .form-label { font-size: 0.75rem; color: var(--text-muted); font-weight: 600; }
    .form-input {
      min-height: 44px;
      padding: 0.6rem 0.85rem;
      background-color: #0b0f19;
      border: 1px solid var(--card-border);
      border-radius: 8px;
      color: #fff;
      font-size: 0.9rem;
      outline: none;
    }
    .form-input:focus { border-color: var(--primary); }

    .toast {
      position: fixed;
      bottom: 1.25rem;
      right: 1.25rem;
      background-color: #1f2937;
      color: #fff;
      padding: 0.75rem 1.25rem;
      border-radius: 8px;
      border: 1px solid var(--card-border);
      font-size: 0.82rem;
      display: flex;
      align-items: center;
      gap: 0.5rem;
      box-shadow: 0 10px 25px rgba(0,0,0,0.5);
      transform: translateY(150%);
      transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
      z-index: 100;
    }
    .toast.show { transform: translateY(0); }
  </style>
</head>
<body>
  <header>
    <div class="header-content">
      <div class="brand">
        <div class="logo-badge">PQ</div>
        <div class="brand-text">
          <h1>Monitor de Qualidade de Energia</h1>
          <p>IEC 61000-4-30 • DSP Dual-Core ESP32 (60 Hz)</p>
        </div>
      </div>
      <div class="header-status">
        <span class="badge" id="sourceBadge">
          <span style="color:var(--primary);">●</span> Fonte: <strong id="sourceText">ADC (GPIO 34)</strong>
        </span>
        <span class="badge">
          <span class="badge-dot disconnected" id="wsDot"></span>
          <span id="wsStatusText">Conectando...</span>
        </span>
      </div>
    </div>
  </header>

  <div class="container">
    <section class="metrics-grid">
      <div class="card">
        <div class="card-title">Tensão RMS</div>
        <div class="card-value"><span id="vRmsValue">---.-</span><span class="card-unit">V</span></div>
        <div class="card-subtext">Ref: <span id="vRefSubtext">127.0</span> V</div>
      </div>
      <div class="card">
        <div class="card-title">Normalizada (pu)</div>
        <div class="card-value"><span id="puValue">-.---</span><span class="card-unit">pu</span></div>
        <div class="card-subtext" id="puDeltaText">Nominal: 1.000 pu</div>
      </div>
      <div class="card status-card state-normal" id="statusCard">
        <div class="card-title"><span>Estado da Rede</span><span class="status-indicator" id="statusPulse">●</span></div>
        <div class="card-value" style="font-size:1.4rem;" id="stateTitle">NORMAL</div>
        <div class="card-subtext" id="stateSubtext">Operação em Regime Permanente</div>
      </div>
      <div class="card">
        <div class="card-title">Frequência / Ciclos</div>
        <div class="card-value"><span id="freqValue">60.0</span><span class="card-unit">Hz</span></div>
        <div class="card-subtext">Ciclo: <span id="cycleCounter">0</span></div>
      </div>
    </section>

    <section class="chart-section">
      <div class="chart-container">
        <div class="chart-header">
          <div>
            <h2 class="chart-title">Envelope de Tensão em Tempo Real (RMS pu)</h2>
            <p style="font-size:0.75rem; color:var(--text-muted);">Amostragem: 64 amostras/ciclo (3840 Hz) no Core 1</p>
          </div>
          <div class="chart-legend">
            <span class="legend-item"><span class="legend-line" style="background:#ef4444; border-top:2px dashed #ef4444;"></span> Swell (≥ 1.10)</span>
            <span class="legend-item"><span class="legend-line" style="background:#10b981;"></span> Nominal (1.00)</span>
            <span class="legend-item"><span class="legend-line" style="background:#f59e0b; border-top:2px dashed #f59e0b;"></span> Sag (≤ 0.90)</span>
          </div>
        </div>
        <div class="canvas-wrap">
          <canvas id="scopeCanvas"></canvas>
        </div>
      </div>
    </section>

    <section class="test-bench">
      <div class="bench-title">Bancada de Testes de Distúrbios (Injeção em Tempo Real)</div>
      <p class="bench-desc">Gere distúrbios controlados pelo sintetizador do Core 1 ou retorne ao ADC físico:</p>
      <div class="button-group">
        <button class="btn btn-nominal" onclick="triggerScenario('nominal', 1.0, 0)">✔ Nominal (1.00 pu)</button>
        <button class="btn btn-sag" onclick="triggerScenario('sag', 0.75, 400)">▼ Sag (0.75 pu - 400ms)</button>
        <button class="btn btn-swell" onclick="triggerScenario('swell', 1.25, 400)">▲ Swell (1.25 pu - 400ms)</button>
        <button class="btn btn-hardware" onclick="triggerScenario('adc', 1.0, 0)">⚡ Modo Real (GPIO 34)</button>
      </div>
    </section>

    <section class="history-section">
      <div class="history-card">
        <div class="history-header">
          <div>
            <h2 style="font-size:1rem; font-weight:700;">Histórico de Eventos IEC 61000-4-30</h2>
            <p style="font-size:0.75rem; color:var(--text-muted);">Eventos consolidados com histerese</p>
          </div>
          <div class="history-actions">
            <button class="btn btn-primary" onclick="exportToCSV()">Exportar CSV</button>
            <button class="btn btn-secondary" onclick="clearEventHistory()">Limpar</button>
          </div>
        </div>
        <div class="table-responsive">
          <table id="eventsTable">
            <thead>
              <tr>
                <th># ID</th><th>Classe</th><th>Horário</th><th>Duração (ms)</th><th>Duração (ciclos)</th><th>Tensão Extrema</th><th>Relação (pu)</th>
              </tr>
            </thead>
            <tbody id="eventsTableBody">
              <tr id="emptyTableRow"><td colspan="7" style="text-align:center; padding:1.5rem; color:var(--text-muted);">Nenhum evento registrado até o momento.</td></tr>
            </tbody>
          </table>
        </div>
      </div>
    </section>

    <section class="config-panel">
      <h2 style="font-size:0.95rem; font-weight:700;">Parâmetros Operacionais (REST API)</h2>
      <form id="configForm" onsubmit="saveConfiguration(event)">
        <div class="form-grid">
          <div class="form-group">
            <label class="form-label">Tensão Ref (V)</label>
            <input type="number" step="0.1" class="form-input" id="cfgVref" value="127.0" required>
          </div>
          <div class="form-group">
            <label class="form-label">Limiar Sag (pu)</label>
            <input type="number" step="0.01" class="form-input" id="cfgSag" value="0.90" required>
          </div>
          <div class="form-group">
            <label class="form-label">Limiar Swell (pu)</label>
            <input type="number" step="0.01" class="form-input" id="cfgSwell" value="1.10" required>
          </div>
          <div class="form-group">
            <label class="form-label">Histerese (pu)</label>
            <input type="number" step="0.005" class="form-input" id="cfgHyst" value="0.02" required>
          </div>
        </div>
        <div style="margin-top:0.85rem; display:flex; justify-content:flex-end;">
          <button type="submit" class="btn btn-primary">Salvar Parâmetros no ESP32</button>
        </div>
      </form>
    </section>
  </div>

  <div class="toast" id="toast">
    <span id="toastIcon">✔</span>
    <span id="toastMsg">Configurações atualizadas!</span>
  </div>

  <script>
    let ws = null;
    const recordedEvents = [];
    let currentVRef = 127.0;
    let sagLimit = 0.90;
    let swellLimit = 1.10;
    const MAX_POINTS = 120;
    const chartBuffer = new Array(MAX_POINTS).fill(1.0);

    window.addEventListener('DOMContentLoaded', () => {
      initCanvas();
      fetchConfig();
      fetchEvents();
      connectWebSocket();
      window.addEventListener('resize', resizeCanvas);
    });

    function connectWebSocket() {
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${protocol}//${window.location.host}/ws`;
      const wsDot = document.getElementById('wsDot');
      const wsStatusText = document.getElementById('wsStatusText');

      ws = new WebSocket(wsUrl);
      ws.onopen = () => {
        wsDot.className = 'badge-dot connected';
        wsStatusText.textContent = 'Conectado';
      };
      ws.onclose = () => {
        wsDot.className = 'badge-dot disconnected';
        wsStatusText.textContent = 'Desconectado';
        setTimeout(connectWebSocket, 2000);
      };
      ws.onerror = () => { ws.close(); };
      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          if (msg.type === 'telemetry') handleTelemetry(msg);
          else if (msg.type === 'event') handleNewEvent(msg);
        } catch (e) {}
      };
    }

    function handleTelemetry(t) {
      document.getElementById('vRmsValue').textContent = t.v_rms.toFixed(1);
      document.getElementById('puValue').textContent = t.pu.toFixed(3);
      document.getElementById('freqValue').textContent = t.freq.toFixed(1);
      document.getElementById('cycleCounter').textContent = t.cycle.toLocaleString();

      const delta = ((t.pu - 1.0) * 100).toFixed(1);
      const sign = delta >= 0 ? '+' : '';
      document.getElementById('puDeltaText').textContent = `Desvio: ${sign}${delta}%`;

      const card = document.getElementById('statusCard');
      const title = document.getElementById('stateTitle');
      const sub = document.getElementById('stateSubtext');

      if (t.state === 1 || t.state === 'SAG') {
        card.className = 'card status-card state-sag';
        title.textContent = 'AFUNDAMENTO (SAG)';
        sub.textContent = `Duração: ${t.active_dur || 0} ms | Mín: ${(t.active_extreme || t.pu).toFixed(3)} pu`;
      } else if (t.state === 2 || t.state === 'SWELL') {
        card.className = 'card status-card state-swell';
        title.textContent = 'ELEVAÇÃO (SWELL)';
        sub.textContent = `Duração: ${t.active_dur || 0} ms | Máx: ${(t.active_extreme || t.pu).toFixed(3)} pu`;
      } else {
        card.className = 'card status-card state-normal';
        title.textContent = 'NORMAL';
        sub.textContent = 'Operação em Regime Permanente';
      }

      chartBuffer.push(t.pu);
      if (chartBuffer.length > MAX_POINTS) chartBuffer.shift();
      drawScope();
    }

    function handleNewEvent(evt) {
      recordedEvents.unshift(evt);
      renderTable();
      showToast(`Evento ${evt.class} #${evt.id} registrado (${evt.duration_ms} ms, ${evt.extreme_pu.toFixed(3)} pu)`);
    }

    function renderTable() {
      const tbody = document.getElementById('eventsTableBody');
      if (recordedEvents.length === 0) {
        tbody.innerHTML = `<tr id="emptyTableRow"><td colspan="7" style="text-align:center; padding:1.5rem; color:var(--text-muted);">Nenhum evento registrado até o momento.</td></tr>`;
        return;
      }
      tbody.innerHTML = recordedEvents.map(e => {
        const isSag = e.class === 'SAG' || e.type === 1;
        const badgeClass = isSag ? 'badge-sag' : 'badge-swell';
        const label = isSag ? 'SAG' : 'SWELL';
        return `
          <tr>
            <td><strong>#${e.id}</strong></td>
            <td><span class="badge-event ${badgeClass}">${label}</span></td>
            <td>${e.timestamp_ms || e.timestamp || 0} ms</td>
            <td><strong>${e.duration_ms}</strong> ms</td>
            <td>${e.duration_cycles || e.cycles || Math.round(e.duration_ms / 16.666)}</td>
            <td>${(e.extreme_voltage || e.extreme_v || 0).toFixed(1)} V</td>
            <td><strong>${(e.extreme_pu).toFixed(3)} pu</strong></td>
          </tr>
        `;
      }).join('');
    }

    const canvas = document.getElementById('scopeCanvas');
    const ctx = canvas.getContext('2d');

    function initCanvas() { resizeCanvas(); drawScope(); }
    function resizeCanvas() {
      const rect = canvas.getBoundingClientRect();
      const dpr = window.devicePixelRatio || 1;
      canvas.width = rect.width * dpr;
      canvas.height = rect.height * dpr;
      ctx.resetTransform();
      ctx.scale(dpr, dpr);
      drawScope();
    }

    function drawScope() {
      const rect = canvas.getBoundingClientRect();
      const w = rect.width;
      const h = rect.height;
      ctx.clearRect(0, 0, w, h);

      const yMin = 0.4;
      const yMax = 1.6;
      const yToPx = (val) => h - ((val - yMin) / (yMax - yMin)) * h;

      ctx.strokeStyle = '#1e293b';
      ctx.lineWidth = 1;
      [0.6, 0.8, 1.0, 1.2, 1.4].forEach(val => {
        const y = yToPx(val);
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      });

      // Swell line
      const ySwell = yToPx(swellLimit);
      ctx.save();
      ctx.strokeStyle = '#ef4444';
      ctx.setLineDash([6, 4]);
      ctx.beginPath(); ctx.moveTo(0, ySwell); ctx.lineTo(w, ySwell); ctx.stroke();
      ctx.restore();

      // Nominal line
      const yNom = yToPx(1.0);
      ctx.save();
      ctx.strokeStyle = '#10b981';
      ctx.setLineDash([2, 4]);
      ctx.beginPath(); ctx.moveTo(0, yNom); ctx.lineTo(w, yNom); ctx.stroke();
      ctx.restore();

      // Sag line
      const ySag = yToPx(sagLimit);
      ctx.save();
      ctx.strokeStyle = '#f59e0b';
      ctx.setLineDash([6, 4]);
      ctx.beginPath(); ctx.moveTo(0, ySag); ctx.lineTo(w, ySag); ctx.stroke();
      ctx.restore();

      // Envelope trace
      const stepX = w / (MAX_POINTS - 1);
      ctx.beginPath();
      for (let i = 0; i < chartBuffer.length; i++) {
        const x = i * stepX;
        const val = Math.max(yMin, Math.min(yMax, chartBuffer[i]));
        const y = yToPx(val);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2.5;
      ctx.stroke();
    }

    function triggerScenario(type, pu, duration) {
      const sourceText = document.getElementById('sourceText');
      sourceText.textContent = (type === 'adc') ? 'ADC (GPIO 34)' : `Sintetizador (${type.toUpperCase()})`;
      fetch('/api/simulate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ scenario: type, pu: pu, duration_ms: duration })
      }).then(() => showToast(`Cenário '${type.toUpperCase()}' acionado.`));
    }

    function exportToCSV() {
      if (recordedEvents.length === 0) { showToast('Nenhum evento para exportar.'); return; }
      let csv = '\uFEFFID;Classe;Timestamp_ms;Duracao_ms;Duracao_Ciclos;Tensao_Extrema_V;Relacao_pu\r\n';
      recordedEvents.forEach(e => {
        const cls = e.class || (e.type === 1 ? 'SAG' : 'SWELL');
        csv += `${e.id};${cls};${e.timestamp_ms||0};${e.duration_ms||0};${e.duration_cycles||0};${(e.extreme_voltage||e.extreme_v||0).toFixed(2)};${(e.extreme_pu||0).toFixed(4)}\r\n`;
      });
      const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `pq_events_iec61000_${new Date().toISOString().slice(0,10)}.csv`;
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      showToast('Relatório CSV exportado!');
    }

    function clearEventHistory() {
      recordedEvents.length = 0;
      renderTable();
      fetch('/api/events/clear', { method: 'POST' }).catch(() => {});
      showToast('Histórico limpo.');
    }

    function fetchConfig() {
      fetch('/api/config').then(r => r.json()).then(cfg => {
        document.getElementById('cfgVref').value = cfg.v_ref;
        document.getElementById('cfgSag').value = cfg.sag_threshold;
        document.getElementById('cfgSwell').value = cfg.swell_threshold;
        document.getElementById('cfgHyst').value = cfg.hysteresis;
        currentVRef = cfg.v_ref;
        sagLimit = cfg.sag_threshold;
        swellLimit = cfg.swell_threshold;
        document.getElementById('vRefSubtext').textContent = currentVRef.toFixed(1);
        drawScope();
      }).catch(() => {});
    }

    function fetchEvents() {
      fetch('/api/events').then(r => r.json()).then(events => {
        if (Array.isArray(events)) { recordedEvents.push(...events); renderTable(); }
      }).catch(() => {});
    }

    function saveConfiguration(event) {
      event.preventDefault();
      const payload = {
        v_ref: parseFloat(document.getElementById('cfgVref').value),
        sag_threshold: parseFloat(document.getElementById('cfgSag').value),
        swell_threshold: parseFloat(document.getElementById('cfgSwell').value),
        hysteresis: parseFloat(document.getElementById('cfgHyst').value)
      };
      fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      }).then(() => {
        currentVRef = payload.v_ref;
        sagLimit = payload.sag_threshold;
        swellLimit = payload.swell_threshold;
        document.getElementById('vRefSubtext').textContent = currentVRef.toFixed(1);
        drawScope();
        showToast('Parâmetros salvos no ESP32!');
      });
    }

    function showToast(msg) {
      const toast = document.getElementById('toast');
      document.getElementById('toastMsg').textContent = msg;
      toast.classList.add('show');
      setTimeout(() => toast.classList.remove('show'), 3500);
    }
  </script>
</body>
</html>
)rawliteral";

#endif // HTML_CONTENT_H
