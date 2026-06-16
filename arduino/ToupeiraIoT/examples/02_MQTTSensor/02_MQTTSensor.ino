/*
 * 02_MQTTSensor — Temperatura e umidade via MQTT
 * ─────────────────────────────────────────────────────────────
 * Hardware:
 *   ESP32 + Sensor DHT22 (mesmo circuito do exemplo 01)
 *
 * Bibliotecas:
 *   - ToupeiraIoT
 *   - DHT sensor library  (Adafruit)
 *   - Adafruit Unified Sensor
 *   - ArduinoJson
 *   - PubSubClient
 *
 * Por que MQTT em vez de HTTP?
 *   MQTT mantém uma conexão persistente com o broker, ideal para
 *   envios frequentes (< 5s). Consome menos bateria e banda que
 *   abrir uma conexão HTTP a cada leitura.
 *
 *   Tópico publicado: iot/{deviceId}/temperatura
 *                     iot/{deviceId}/umidade
 */

#include <ToupeiraIoT.h>
#include <DHT.h>

// ── Credenciais ───────────────────────────────────────────────
const char* DEVICE_ID  = "COLE_SEU_DEVICE_ID_AQUI";
const char* API_KEY    = "COLE_SUA_API_KEY_AQUI";

// ── Rede ──────────────────────────────────────────────────────
const char* WIFI_SSID  = "SeuWiFi";
const char* WIFI_PASS  = "SuaSenha";

// ── MQTT Broker ───────────────────────────────────────────────
const char*    MQTT_HOST = "broker.toupeira.io"; // ou IP local
const uint16_t MQTT_PORT = 1883;

// ── Hardware ──────────────────────────────────────────────────
#define DHT_PIN  4
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);
ToupeiraIoT iot(DEVICE_ID, API_KEY);

const unsigned long INTERVALO_MS = 5000;
unsigned long ultimoEnvio = 0;
uint32_t totalEnviados = 0;

// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   Toupeira IoT — MQTTSensor        ║");
  Serial.println("╚════════════════════════════════════╝\n");

  dht.begin();

  if (!iot.connectWiFi(WIFI_SSID, WIFI_PASS)) {
    Serial.println("ERRO: WiFi falhou.");
    while (true) delay(1000);
  }

  if (!iot.connectMQTT(MQTT_HOST, MQTT_PORT)) {
    Serial.println("ERRO: MQTT falhou. Verifique o broker.");
    // Continua mesmo assim — loop() tentará reconectar
  }

  Serial.printf("\nBroker: %s:%d\n", MQTT_HOST, MQTT_PORT);
  Serial.printf("Tópico: iot/%s/{métrica}\n\n", DEVICE_ID);
}

void loop() {
  iot.loop(); // Mantém a conexão MQTT ativa

  unsigned long agora = millis();
  if (agora - ultimoEnvio >= INTERVALO_MS) {
    ultimoEnvio = agora;

    float temperatura = dht.readTemperature();
    float umidade     = dht.readHumidity();

    if (isnan(temperatura) || isnan(umidade)) {
      Serial.println("[ERRO] Falha ao ler DHT22.");
      return;
    }

    iot.publish("temperatura", temperatura);
    iot.publish("umidade",     umidade);

    totalEnviados++;
    Serial.printf("[#%04u] T:%.1f°C | U:%.1f%%\n",
                  totalEnviados, temperatura, umidade);
  }
}
