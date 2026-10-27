# Metrum PQ Monitor | IEC 61000-4-30 (Sag & Swell)

Protótipo industrial de monitoramento de Qualidade de Energia focado na detecção em tempo real de distúrbios de tensão (**Afundamentos / Sag** e **Elevações / Swell**) em conformidade com as diretrizes da norma **IEC 61000-4-30** em redes de 60 Hz, utilizando o microcontrolador **ESP32** (FreeRTOS Dual-Core) e uma interface web interativa de alta performance.

Desenvolvido para o desafio técnico industrial **Metrum / HackatOhm**.

---

## 1. Destaques da Arquitetura

- **Multitarefa Dual-Core FreeRTOS Dedicada:**
  - **Core 1 (DSP & Aquisição - Prioridade Máxima 24):** Amostragem contínua determinística a **64 amostras por ciclo** (3840 Hz no GPIO 34), eliminação dinâmica de offset DC, cálculo de *True RMS* ciclo a ciclo, normalização em por unidade ($pu = V_{RMS} / V_{ref}$) e máquina de estados com histerese.
  - **Core 0 (Comunicação & Web - Prioridade Baixa 2):** Pilha Wi-Fi (SoftAP e STA), servidor assíncrono `ESPAsyncWebServer`, streaming WebSocket bidirecional a ~10 Hz, endpoints REST e histórico consolidado.
- **Comunicação Inter-Core Segura (*Thread-Safe*):**
  - Filas do FreeRTOS: `xQueueOverwrite` para telemetria fresca (não bloqueante) e `xQueueSend` para eventos consolidados.
  - `xSemaphoreCreateMutex` para atualização de parâmetros em tempo de execução sem paradas de máquina.
- **Interface Web Mobile-First & Offline-Ready:**
  - Totalmente funcional no modo SoftAP (`192.168.4.1`) sem requisições a CDNs externas.
  - Gráfico dinâmico do envelope de tensão via HTML5 Canvas com linhas de limiar tracejadas em 0.90 pu e 1.10 pu.
  - Cards com indicação de estado e animações dinâmicas (Verde: Normal, Âmbar: Sag, Vermelho: Swell).
  - Tabela com histórico consolidado e botão de **Exportação CSV** formatado com BOM UTF-8 (compatível com Microsoft Excel).
  - Bancada didática de testes com injeção de distúrbios virtuais para demonstração instantânea em bancada.
  - Alvos de toque ergonômicos ($\ge 44 \times 44\text{ px}$) e layout 100% testado em telas compactas (a partir de 320 px de largura).

---

## 2. Pinout e Circuito de Condicionamento Analógico

| Função | Pino ESP32 | Descrição |
|---|---|---|
| **Sinal Analógico CA** | **GPIO 34 (ADC1_CH6)** | Entrada exclusiva de conversão analógica (não interfere com Wi-Fi) |
| **GND** | **GND** | Referência comum do sistema |
| **Alimentação** | **VIN / 5V** ou **3V3** | Alimentação do módulo sensor (ZMPT101B ou divisor ativo) |

### Esquema Típico de Condicionamento:
```
 Rede CA (127V / 220V) 
        │
   ┌────┴────┐
   │ Sensor  │ (ex: Módulo ZMPT101B com trimpot de calibração)
   │ZMPT101B │ ou Divisor Resistivo + Offset DC de 1.65V
   └────┬────┘
        ├─── Saída Analógica (Sinal centrado em 1.65V, pico a pico < 3.0V)
        │
        └───> GPIO 34 do ESP32 (ADC1 Channel 6)
```

---

## 3. Estrutura do Projeto

```
fervent-archimedes/
├── platformio.ini              # Configurações de build do PlatformIO
├── include/
│   ├── types.h                 # Estruturas de dados (TelemetryData, PowerQualityEvent)
│   ├── dsp_engine.h            # Declaração do motor de amostragem e máquina de estados
│   ├── web_server.h            # Declaração do servidor HTTP, WebSocket e REST
│   └── html_content.h          # Fallback da interface web em PROGMEM Flash
├── src/
│   ├── main.cpp                # Ponto de entrada, alocação de tarefas nos núcleos do ESP32
│   ├── dsp_engine.cpp          # Implementação de amostragem a 3840 Hz e IEC 61000-4-30
│   └── web_server.cpp          # Handlers WebSocket e rotas REST API
├── data/
│   └── index.html              # Dashboard Web responsivo (HTML5 + CSS + Canvas)
└── docs/
    └── architecture.md         # Documentação de engenharia detalhada
```

---

## 4. Instruções de Compilação e Gravação

### Opção A: Utilizando PlatformIO (Recomendado)
1. Instale o **VS Code** com a extensão **PlatformIO IDE**.
2. Abra a pasta do projeto no VS Code.
3. Conecte o ESP32 à porta USB do computador.
4. Execute o build e upload:
   ```bash
   # Compilar e gravar o firmware no ESP32
   pio run --target upload

   # (Opcional) Gravar a pasta data/ no LittleFS
   pio run --target uploadfs

   # Abrir o monitor serial (115200 bps)
   pio device monitor
   ```

> **Nota:** Se você não executar o comando `uploadfs`, o firmware ativará automaticamente a página web de contingência embutida na memória Flash (`PROGMEM`), funcionando 100% de primeira!

### Opção B: Utilizando Arduino IDE (v2.x)
1. Abra a IDE do Arduino e selecione a placa **ESP32 Dev Module**.
2. No gerenciador de bibliotecas da IDE, instale:
   - `ArduinoJson` (v7.x por Benoît Blanchon)
   - `ESPAsyncWebServer` (por mathieucarbou / me-no-dev)
   - `AsyncTCP` (por mathieucarbou)
3. Abra o arquivo `src/main.cpp` (ou renomeie `main.cpp` para uma pasta de mesmo nome `.ino`).
4. Conecte o ESP32 e clique em **Carregar**.

---

## 5. Como Testar os 3 Cenários Obrigatórios

Após gravar o firmware, conecte seu notebook ou smartphone à rede Wi-Fi gerada pelo ESP32:
- **Rede Wi-Fi (SSID):** `ESP32-PowerQuality`
- **Senha:** `metrum123`
- **Endereço do Painel:** Acesse no navegador `http://192.168.4.1`

*(Se configurado em modo STA, o ESP32 exibirá o endereço IP obtido via DHCP no Monitor Serial).*

### Teste 1: Regime Permanente Nominal (1.00 pu)
- **Ação:** Clique no botão verde **"Nominal (1.00 pu)"** no painel *Bancada de Testes de Distúrbios*.
- **Comportamento Esperado:**
  - O mostrador RMS estabiliza em **~127.0 V** ($\pm 0.5\text{ V}$).
  - A tensão por unidade indica **1.000 pu** (Desvio: 0.0%).
  - O Card de Estado exibe **NORMAL** com contorno verde e texto *"Operação em Regime Permanente"*.
  - O traço do gráfico repousa na linha central tracejada verde (1.00 pu).

### Teste 2: Afundamento de Tensão / Sag ($\le 0.90\text{ pu}$)
- **Ação:** Clique no botão âmbar **"Afundamento (Sag 0.75 pu)"**.
- **Comportamento Esperado:**
  - A tensão cai imediatamente para **~95.3 V** (0.75 pu).
  - O Card de Estado muda dinamicamente para cor âmbar pulsante com o título **AFUNDAMENTO (SAG)**.
  - O envelope cai abaixo da linha de limiar tracejada âmbar em 0.90 pu.
  - Após 400 ms (tempo do pulso simulado), a tensão se recupera para 1.00 pu. Ao cruzar **0.92 pu** (limiar de 0.90 pu + histerese de 0.02 pu), o evento é finalizado.
  - Uma nova linha é inserida instantaneamente na **Tabela de Histórico de Eventos** indicando:
    - Classe: `SAG`
    - Duração: `~400 ms` (~24 ciclos de 60 Hz)
    - Tensão Extrema: `~95.3 V`
    - Relação: `0.750 pu`

### Teste 3: Elevação de Tensão / Swell ($\ge 1.10\text{ pu}$)
- **Ação:** Clique no botão vermelho **"Elevação (Swell 1.25 pu)"**.
- **Comportamento Esperado:**
  - A tensão sobe imediatamente para **~158.8 V** (1.25 pu).
  - O Card de Estado comuta para cor vermelha com o título **ELEVAÇÃO (SWELL)**.
  - O traço do gráfico ultrapassa a linha tracejada vermelha superior (1.10 pu).
  - Após 400 ms, a tensão retorna a 1.00 pu. Ao atingir **1.08 pu** (limiar de 1.10 pu - histerese de 0.02 pu), o evento de Swell é consolidado e registrado na tabela.

### Teste com Sinal Real via Hardware (Gerador de Sinais / Variac)
- Clique no botão **"Modo Real (ADC GPIO 34)"**.
- O sistema comuta para leitura contínua do pino GPIO 34 a 3840 Hz.
- Ao variar a amplitude no gerador de funções ou variac, observe o detector IEC 61000-4-30 registrar os eventos físicos reais com precisão de ciclos.

---

## 6. Exportação de Relatórios e Configurações Dinâmicas

1. **Exportação CSV:**
   - Clique em **"Exportar CSV"** na tabela de histórico.
   - O arquivo `pq_events_iec61000_AAAA-MM-DD.csv` é gerado contendo cabeçalho estruturado e codificação UTF-8 BOM para abertura nativa no Excel ou importação em sistemas SCADA / Power BI.
2. **Ajuste de Parâmetros sem Regravar o ESP32:**
   - No painel inferior, altere $V_{ref}$ para 220V ou modifique os limiares de Sag (ex: 0.85 pu) e Swell (ex: 1.15 pu).
   - Clique em **"Salvar Parâmetros no ESP32"**. A chamada `POST /api/config` atualiza os limiares no microcontrolador via mutex em menos de 5 milissegundos.
