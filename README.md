# Toupeira IoT — Developer SDK

Bibliotecas e exemplos oficiais para integrar dispositivos à plataforma **Toupeira IoT**.

---

## Conteúdo

| Pasta | Descrição |
|---|---|
| `arduino/ToupeiraIoT/` | Biblioteca Arduino/ESP32 (instalável via Library Manager) |
| `python/` | SDK Python — síncrono e assíncrono (Raspberry Pi, Linux, Mac) |
| `http/` | Exemplos curl + coleção Postman |

---

## ESP32 / Arduino

### Instalação

1. Baixe este repositório como ZIP: **Code → Download ZIP**
2. No Arduino IDE: **Sketch → Incluir Biblioteca → Adicionar .ZIP**

**Dependências** (instale pelo Library Manager):
- `ArduinoJson` >= 6.x
- `PubSubClient` >= 2.8
- `DHT sensor library` (Adafruit) — exemplos DHT22
- `Adafruit BME280 Library` — exemplo MultiSensor
- `FastLED` — exemplo de atuador LED

### Exemplos

| Exemplo | Hardware | Protocolo | O que faz |
|---|---|---|---|
| `01_BasicSensor` | ESP32 + DHT22 | HTTP | Temperatura e umidade, envio simples |
| `02_MQTTSensor` | ESP32 + DHT22 | MQTT | Mesmo sensor, protocolo mais eficiente |
| `03_MultiSensor` | ESP32 + BME280 | MQTT | 4 métricas simultâneas (temp, umid, pressão, altitude) |
| `04_Actuator` | ESP32 + WS2812B | MQTT | Fita LED controlada pela plataforma (6 efeitos) |
| `05_OTA` | ESP32 + DHT22 | HTTP + OTA | Sensor com update de firmware via WiFi |

### Início rápido

```cpp
#include <ToupeiraIoT.h>

ToupeiraIoT iot("SEU_DEVICE_ID", "SUA_API_KEY");

void setup() {
  iot.connectWiFi("SeuWiFi", "SuaSenha");
  iot.setApiUrl("https://api.toupeira.io");
}

void loop() {
  iot.publishHTTP("temperatura", 23.5);
  delay(10000);
}
```

---

## Python

### Instalação

```bash
pip install requests paho-mqtt          # dependências mínimas
pip install aiohttp asyncio-mqtt        # para cliente assíncrono

# Instalação local do pacote
pip install -e ./python
```

### Exemplos

| Exemplo | Ambiente | O que faz |
|---|---|---|
| `01_basic_http.py` | Qualquer Python 3.8+ | Métricas do sistema via HTTP |
| `02_mqtt_sensor.py` | Qualquer Python 3.8+ | Loop MQTT com reconexão automática |
| `03_raspberry_dht.py` | Raspberry Pi | DHT22 via GPIO, suporta Adafruit_DHT e circuitpython |
| `04_async_sensor.py` | Python 3.8+ (asyncio) | Múltiplos sensores em paralelo sem bloquear |

### Início rápido (HTTP)

```python
from toupeira_iot import ToupeiraIoT

iot = ToupeiraIoT("SEU_DEVICE_ID", "SUA_API_KEY")
iot.set_api_url("https://api.toupeira.io")

iot.publish_http("temperatura", 23.5)
iot.publish_batch({"temperatura": 23.5, "umidade": 68.2})
```

### Início rápido (MQTT)

```python
from toupeira_iot import ToupeiraIoT

iot = ToupeiraIoT("SEU_DEVICE_ID", "SUA_API_KEY")
iot.connect_mqtt("broker.toupeira.io")
iot.publish("temperatura", 23.5)
iot.loop_forever()   # mantém a conexão ativa
```

### Início rápido (assíncrono)

```python
import asyncio
from toupeira_iot import ToupeiraIoTAsync

async def main():
    iot = ToupeiraIoTAsync("SEU_DEVICE_ID", "SUA_API_KEY")
    iot.set_api_url("https://api.toupeira.io")
    await iot.publish_http("temperatura", 23.5)
    await iot.close()

asyncio.run(main())
```

---

## HTTP / REST

### Coleção Postman

Importe o arquivo `http/postman_collection.json` no Postman e configure as variáveis:

| Variável | Exemplo |
|---|---|
| `base_url` | `https://api.toupeira.io` |
| `user_token` | JWT obtido no login |
| `device_id` | UUID do seu dispositivo |
| `api_key` | API Key do dispositivo |

### Exemplos curl

```bash
# Enviar leitura
curl -X POST https://api.toupeira.io/telemetry \
  -H "X-Device-Key: SUA_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"deviceId":"SEU_DEVICE_ID","metric":"temperatura","value":23.5}'

# Enviar batch (múltiplas métricas)
curl -X POST https://api.toupeira.io/telemetry/batch \
  -H "X-Device-Key: SUA_API_KEY" \
  -H "Content-Type: application/json" \
  -d '[{"deviceId":"ID","metric":"temperatura","value":23.5},{"deviceId":"ID","metric":"umidade","value":68.2}]'

# Heartbeat
curl -X POST https://api.toupeira.io/devices/SEU_DEVICE_ID/heartbeat \
  -H "X-Device-Key: SUA_API_KEY"
```

Veja todos os exemplos em [`http/curl_examples.sh`](http/curl_examples.sh).

---

## Referência da API Arduino

```cpp
// Construtor
ToupeiraIoT iot(DEVICE_ID, API_KEY);

// WiFi
iot.connectWiFi("SSID", "senha");
iot.isWiFiConnected();              // → bool

// HTTP
iot.setApiUrl("https://api.toupeira.io");
iot.publishHTTP("metrica", 23.5);   // → bool
iot.publishHTTP("status", "ativo"); // → bool (string)
iot.heartbeat();                    // → bool

// MQTT
iot.connectMQTT("broker.toupeira.io");
iot.publish("metrica", 23.5);       // → bool
iot.onCommand(callback);            // registra callback de atuador
iot.loop();                         // chame no loop() sempre
iot.isConnected();                  // → bool
```

## Referência da API Python

```python
iot = ToupeiraIoT(device_id, api_key)

# HTTP
iot.set_api_url(url)
iot.publish_http(metric, value)     # → bool
iot.publish_batch({metric: value})  # → bool
iot.heartbeat()                     # → bool

# MQTT
iot.connect_mqtt(host, port=1883)
iot.publish(metric, value)          # → bool
iot.on_command(callback)            # callback(command, params_dict)
iot.loop_forever()                  # bloqueia (use em threads)
iot.disconnect()
iot.is_connected()                  # → bool
```

---

## Tópicos MQTT

| Direção | Tópico | Uso |
|---|---|---|
| **Publish** | `iot/{deviceId}/{metrica}` | Envio de dados de sensores |
| **Subscribe** | `actuators/{deviceId}/set` | Recebe comandos da plataforma |

---

## Licença

MIT — Toupeira IoT
