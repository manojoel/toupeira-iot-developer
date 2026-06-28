/*
 * 03_MultiSensor — Sensor Ambiental com BME280 via MQTT
 * ─────────────────────────────────────────────────────────────
 * Grupo da plataforma : Sensor Ambiental
 * Protocolo           : MQTT
 * Variáveis enviadas  : temperatura (°C) · umidade (%) · pressao (hPa)
 *
 * Hardware:
 *   ESP32 + BME280 (I²C)
 *
 *   BME280 → ESP32
 *   VCC    → 3V3
 *   GND    → GND
 *   SDA    → GPIO 21
 *   SCL    → GPIO 22
 *
 *   Endereço I²C padrão: 0x76  (jumper SDO em GND)
 *                         0x77  (jumper SDO em VCC)
 *
 * Bibliotecas:
 *   - ToupeiraIoT
 *   - Adafruit BME280 Library
 *   - Adafruit Unified Sensor
 *   - ArduinoJson
 *   - PubSubClient
 *
 * Métricas enviadas via MQTT:
 *   temperatura  (°C)
 *   umidade      (%)
 *   pressao      (hPa)
 *   altitude     (m, relativa ao nível do mar padrão)
 */

#include <ToupeiraIoT.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

// ── Credenciais ───────────────────────────────────────────────
const char* DEVICE_ID  = "COLE_SEU_DEVICE_ID_AQUI";
const char* API_KEY    = "COLE_SUA_API_KEY_AQUI";

// ── Rede ──────────────────────────────────────────────────────
const char* WIFI_SSID  = "SeuWiFi";
const char* WIFI_PASS  = "SuaSenha";
const char* MQTT_HOST  = "broker.toupeira.io";

// ── BME280 ────────────────────────────────────────────────────
#define BME280_ADDR 0x76   // Mude para 0x77 se necessário

// Pressão ao nível do mar da sua localidade (hPa).
// Consulte https://www.inmet.gov.br para o valor correto.
#define PRESSAO_NIVEL_MAR 1013.25

Adafruit_BME280 bme;
ToupeiraIoT iot(DEVICE_ID, API_KEY);

const unsigned long INTERVALO_MS = 15000;
unsigned long ultimoEnvio = 0;

// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   Toupeira IoT — MultiSensor       ║");
  Serial.println("║          BME280 via MQTT            ║");
  Serial.println("╚════════════════════════════════════╝\n");

  Wire.begin();

  if (!bme.begin(BME280_ADDR)) {
    Serial.println("ERRO: BME280 não encontrado!");
    Serial.println("  - Verifique a ligação I²C (SDA=21, SCL=22)");
    Serial.printf("  - Verifique o endereço (0x%02X)\n", BME280_ADDR);
    while (true) delay(1000);
  }

  // Configuração para leituras estáveis (indoor)
  bme.setSampling(
    Adafruit_BME280::MODE_NORMAL,
    Adafruit_BME280::SAMPLING_X2,   // temperatura
    Adafruit_BME280::SAMPLING_X16,  // pressão
    Adafruit_BME280::SAMPLING_X1,   // umidade
    Adafruit_BME280::FILTER_X16,
    Adafruit_BME280::STANDBY_MS_500
  );

  Serial.println("BME280 inicializado!");

  if (!iot.connectWiFi(WIFI_SSID, WIFI_PASS)) {
    Serial.println("ERRO: WiFi falhou.");
    while (true) delay(1000);
  }

  iot.connectMQTT(MQTT_HOST);

  Serial.println("\nIniciando envio de métricas...\n");
}

void loop() {
  iot.loop();

  unsigned long agora = millis();
  if (agora - ultimoEnvio >= INTERVALO_MS) {
    ultimoEnvio = agora;

    float temperatura = bme.readTemperature();
    float umidade     = bme.readHumidity();
    float pressao     = bme.readPressure() / 100.0F;
    float altitude    = bme.readAltitude(PRESSAO_NIVEL_MAR);

    Serial.printf("T:%.1f°C  U:%.1f%%  P:%.1fhPa  Alt:%.1fm\n",
                  temperatura, umidade, pressao, altitude);

    iot.publish("temperatura", temperatura);
    iot.publish("umidade",     umidade);
    iot.publish("pressao",     pressao);
    iot.publish("altitude",    altitude);
  }
}
