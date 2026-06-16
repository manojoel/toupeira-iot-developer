# Toupeira IoT — Developer SDK

Bibliotecas e exemplos oficiais para integrar dispositivos à plataforma **Toupeira IoT**.

---

## Conteúdo

| Pasta | Descrição |
|---|---|
| `arduino/ToupeiraIoT/` | Biblioteca Arduino/ESP32 (instalável via Library Manager) |
| `python/` | SDK Python para Raspberry Pi e Linux *(em breve)* |
| `http/` | Exemplos curl + coleção Postman *(em breve)* |

---

## Início Rápido — ESP32

### 1. Instale as dependências

No Arduino IDE, vá em **Ferramentas → Gerenciar Bibliotecas** e instale:

- `ToupeiraIoT` (este repositório)
- `ArduinoJson` >= 6.x
- `PubSubClient` >= 2.8
- `DHT sensor library` (Adafruit) — para exemplos com DHT22
- `Adafruit BME280 Library` — para o exemplo MultiSensor
- `FastLED` — para o exemplo de atuador LED

### 2. Instale a biblioteca manualmente

1. Baixe este repositório como ZIP: **Code → Download ZIP**
2. No Arduino IDE: **Sketch → Incluir Biblioteca → Adicionar .ZIP**
3. Selecione o arquivo baixado

### 3. Cadastre um dispositivo

1. Acesse a plataforma e crie um dispositivo
2. Copie o **Device ID** (UUID) e a **API Key** gerados
3. Cole nas constantes do sketch:

```cpp
const char* DEVICE_ID = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";
const char* API_KEY   = "tiot_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
```

---

## Exemplos

### 01 — BasicSensor
O mais simples possível. Lê temperatura e umidade de um DHT22 e envia via **HTTP POST** a cada 10 segundos.

**Ideal para:** primeiro teste, redes com firewall, dispositivos com WiFi instável.

```
Hardware: ESP32 + DHT22 (GPIO 4)
Protocolo: HTTP
```

---

### 02 — MQTTSensor
Mesmo hardware do exemplo 01, mas usando **MQTT** para envio mais eficiente. Mantém conexão persistente com o broker e tenta reconectar automaticamente.

**Ideal para:** envios frequentes (< 5s), redes estáveis, baixo consumo de energia.

```
Hardware: ESP32 + DHT22 (GPIO 4)
Protocolo: MQTT
```

---

### 03 — MultiSensor
Sensor **BME280** via I²C. Envia temperatura, umidade, pressão atmosférica e altitude via MQTT.

**Ideal para:** estações meteorológicas, monitoramento ambiental.

```
Hardware: ESP32 + BME280 (SDA=21, SCL=22)
Protocolo: MQTT
Métricas: temperatura, umidade, pressao, altitude
```

---

### 04 — Actuator
Controla uma **fita LED WS2812B** via comandos da plataforma. Recebe comandos MQTT (cor, brilho, efeito, velocidade) e renderiza efeitos em tempo real.

**Efeitos suportados:** `solid` · `blink` · `fade` · `pulse` · `rainbow` · `chase`

```
Hardware: ESP32 + Fita WS2812B (GPIO 5)
Protocolo: MQTT (subscribe)
Controle: Aba "Atuadores" da plataforma
```

---

### 05 — OTA
Combina sensor DHT22 + **atualização de firmware via WiFi** (Over-The-Air). Após o primeiro upload via USB, os próximos podem ser feitos sem cabo.

```
Hardware: ESP32 + DHT22 (GPIO 4)
Protocolo: HTTP + OTA
```

---

## Referência da API

### Construtor

```cpp
ToupeiraIoT iot(DEVICE_ID, API_KEY);
```

### WiFi

```cpp
iot.connectWiFi("SSID", "senha");          // Conecta e aguarda (timeout 15s)
iot.isWiFiConnected();                     // → bool
```

### HTTP

```cpp
iot.setApiUrl("https://api.toupeira.io");
iot.publishHTTP("temperatura", 23.5);      // → bool
iot.publishHTTP("status", "ativo");        // → bool (string)
iot.heartbeat();                           // Notifica dispositivo online → bool
```

### MQTT

```cpp
iot.connectMQTT("broker.toupeira.io");     // porta padrão 1883
iot.publish("temperatura", 23.5);          // → bool
iot.publish("status", "ativo");            // → bool (string)
iot.onCommand(callback);                   // Registra callback de atuador
iot.loop();                                // Chame no loop() sempre
iot.isConnected();                         // → bool
```

### Callback de comandos (atuadores)

```cpp
void onCommand(const char* command, JsonObject& params) {
  bool  power      = params["power"];
  int   brightness = params["brightness"]; // 0-100
  const char* color = params["color"];     // "#RRGGBB"
  const char* effect = params["effect"];   // "solid", "rainbow", ...
}

iot.onCommand(onCommand);
```

---

## Tópicos MQTT

| Direção | Tópico | Uso |
|---|---|---|
| Publish | `iot/{deviceId}/{metrica}` | Envio de dados de sensores |
| Subscribe | `actuators/{deviceId}/set` | Recebe comandos da plataforma |

---

## Licença

MIT — Toupeira IoT
