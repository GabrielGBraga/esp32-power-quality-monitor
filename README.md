# Metrum PQ System | Monitor Multi-ESP32 (IEC 61000-4-30) & Servidor Python :3000

Sistema industrial distribuído de monitoramento de Qualidade de Energia focado na detecção em tempo real de **Afundamentos (Sag)** e **Elevações (Swell)** de tensão conforme a norma **IEC 61000-4-30** em redes de 60 Hz.

O sistema é desacoplado em duas camadas:
1. **Sondas IoT ESP32 (Edge DSP):** Microcontroladores executando amostragem contínua a **64 amostras/ciclo (3840 Hz)** no Core 1 e atuando como clientes WebSocket no Core 0.
2. **Servidor Central Python (Porta 3000):** Backend assíncrono em FastAPI capaz de receber fluxos de múltiplos ESP32s simultâneos, gerenciar histórico consolidado e fornecer um dashboard web interativo em tempo real.

---

## 1. Visão Geral da Arquitetura Desacoplada

```
  ┌───────────────────────┐            ┌───────────────────────┐
  │  ESP32 Sonda 01       │            │  ESP32 Sonda 02       │
  │  (Subestação Norte)   │            │  (Transformador 02)   │
  └──────────┬────────────┘            └──────────┬────────────┘
             │                                    │
             │ ws://<SERVER_IP>:3000/ws/device    │
             └─────────────────┬──────────────────┘
                               ▼
               ┌──────────────────────────────┐
               │    SERVIDOR CENTRAL PYTHON   │
               │        (FastAPI :3000)       │
               └───────────────┬──────────────┘
                               │ WebSocket /ws/ui & HTTP
                               ▼
               ┌──────────────────────────────┐
               │   DASHBOARD WEB INDUSTRIAL   │
               │   - Seletor de Sondas        │
               │   - Envelope Gráfico Canvas  │
               │   - Histórico & Export CSV   │
               └──────────────────────────────┘
```

---

## 2. Como Executar o Servidor Central Python

### 2.1 Pré-requisitos
- Python 3.9+ instalado.
- Instale as dependências:
  ```bash
  pip install -r server/requirements.txt
  ```

### 2.2 Inicialização do Servidor
No diretório raiz do projeto, execute:
```bash
python server/app.py
```
O servidor inicializará escutando em **todas as interfaces de rede na porta 3000**:
- **Dashboard Web:** `http://localhost:3000` (ou `http://<IP_DA_MAQUINA>:3000`)
- **Endpoint de Sondas ESP32:** `ws://<IP_DA_MAQUINA>:3000/ws/device?device_id=<ID_DA_SONDA>`

### 2.3 Simulação de Sondas Virtuais (Sem Hardware)
Para demonstrar o sistema multi-sonda imediatamente no laboratório ou banca avaliadora:
- Abra uma nova janela de terminal e execute:
  ```bash
  python server/simulator.py --device ESP32-SUBESTACAO-01
  ```
- Para adicionar um segundo nó simultâneo:
  ```bash
  python server/simulator.py --device ESP32-TRAFO-02 --vref 220.0
  ```
O painel em `http://localhost:3000` reconhecerá instantaneamente as sondas e permitirá alternar a visualização pelo seletor no topo da página.

---

## 3. Configuração e Gravação da Sonda ESP32

### 3.1 Configuração dos Parâmetros no Firmware
No arquivo [`src/main.cpp`](file:///c:/Users/ggoes/Documents/antigravity/fervent-archimedes/src/main.cpp), ajuste as variáveis de rede e alvo:

```cpp
#define WIFI_SSID     "Nome_Do_Seu_Roteador"
#define WIFI_PASSWORD "Senha_Do_Seu_Roteador"

// IP da máquina onde o server/app.py está rodando
#define SERVER_HOST   "192.168.1.100" 
#define SERVER_PORT   3000

// Identificador único da sonda instalada
#define DEVICE_ID     "ESP32-SUBESTACAO-01"
```

### 3.2 Compilação e Gravação com PlatformIO
Conecte a placa ESP32 via USB e execute:
```bash
pio run --target upload
pio device monitor -b 115200
```
Ao inicializar, a tarefa no Core 0 conectará ao Wi-Fi e abrirá o canal WebSocket com o servidor central Python na porta 3000.

---

## 4. Roteiro de Teste dos 3 Cenários Obrigatórios

Com o servidor rodando e ao menos uma sonda (física ou virtual) conectada:

| Cenário | Procedimento no Painel Web | Resposta Esperada |
|---|---|---|
| **1. Nominal (1.00 pu)** | Selecione a sonda e clique no botão **"Nominal (1.00 pu)"**. | RMS estabiliza em $127.0\text{ V} \pm 0.5\text{ V}$ ($1.000\text{ pu}$). Card exibe **NORMAL** verde e o gráfico segue a linha verde central. |
| **2. Afundamento (Sag)** | Clique no botão **"Afundamento (Sag 0.75 pu)"** (pulso de 400 ms). | Card muda para **AFUNDAMENTO (SAG)** âmbar pulsante. O envelope cai abaixo de 0.90 pu. Ao cruzar 0.92 pu (histerese), o evento é injetado na tabela: `SAG`, `~400 ms`, `~24 ciclos`, `~95.3 V`, `0.750 pu`. |
| **3. Elevação (Swell)** | Clique no botão **"Elevação (Swell 1.25 pu)"** (pulso de 400 ms). | Card muda para **ELEVAÇÃO (SWELL)** vermelho. O envelope ultrapassa 1.10 pu. Ao retornar abaixo de 1.08 pu (histerese), o evento é gravado: `SWELL`, `~400 ms`, `~24 ciclos`, `~158.8 V`, `1.250 pu`. |

---

## 5. Exportação de Relatórios e Ajustes Dinâmicos

1. **Exportar CSV:**
   - Clique em **"Exportar CSV"** na tabela de histórico. O relatório padronizado RFC 4180 com codificação UTF-8 BOM será baixado com todas as ocorrências de todas as sondas.
2. **Ajuste de Parâmetros Remoto:**
   - No rodapé do painel, modifique os limiares e clique em **"Salvar Parâmetros no ESP32"**. O servidor envia uma mensagem de comando via WebSocket para o microcontrolador selecionado, aplicando os novos limiares sem reiniciar a máquina.
