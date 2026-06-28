/*
 * 01_BasicSensor — Sensor Ambiental com DHT22 via HTTP
 * ─────────────────────────────────────────────────────────────
 * Grupo da plataforma : Sensor Ambiental
 * Protocolo           : HTTP
 * Variáveis enviadas  : temperatura (°C) · umidade (%)
 *
 * Hardware:
 *   ESP32 (qualquer variante) + Sensor DHT22
 *
 *   DHT22 → ESP32
 *   VCC   → 3V3
 *   GND   → GND
 *   DATA  → GPIO 4  (com resistor pull-up 10kΩ para 3V3)
 *
 * Bibliotecas (instale pelo Library Manager):
 *   - ToupeiraIoT
 *   - DHT sensor library  (Adafruit)
 *   - Adafruit Unified Sensor
 *   - ArduinoJson
 *
 * Como começar:
 *   1. Crie um dispositivo na plataforma Toupeira IoT
 *   2. Copie o Device ID e a API Key gerados
 *   3. Preencha as constantes abaixo
 *   4. Faça upload e abra o Monitor Serial (115200 baud)
 */

#include <ToupeiraIoT.h>
#include <DHT.h>

// ── Credenciais do dispositivo (obtidas na plataforma) ────────
const char* DEVICE_ID = "COLE_SEU_DEVICE_ID_AQUI";
const char* API_KEY   = "COLE_SUA_API_KEY_AQUI";

// ── Rede WiFi ─────────────────────────────────────────────────
const char* WIFI_SSID = "SeuWiFi";
const char* WIFI_PASS = "SuaSenha";

// ── URL da plataforma ─────────────────────────────────────────
// Produção:      "https://api.toupeira.io"
// Local (dev):   "http://192.168.1.100:3000"
const char* API_URL = "https://api.toupeira.io";

// ── Hardware ──────────────────────────────────────────────────
#define DHT_PIN  4
#define DHT_TYPE DHT22
#define LED_PIN  2   // LED onboard: pisca ao enviar dados

DHT dht(DHT_PIN, DHT_TYPE);
ToupeiraIoT iot(DEVICE_ID, API_KEY);

const unsigned long INTERVALO_MS = 10000; // Envia a cada 10 segundos
unsigned long ultimoEnvio = 0;

// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   Toupeira IoT — BasicSensor       ║");
  Serial.println("╚════════════════════════════════════╝\n");

  pinMode(LED_PIN, OUTPUT);
  dht.begin();

  if (!iot.connectWiFi(WIFI_SSID, WIFI_PASS)) {
    Serial.println("ERRO: WiFi falhou. Verifique SSID e senha.");
    while (true) delay(1000);
  }

  iot.setApiUrl(API_URL);

  Serial.printf("\nDevice ID : %s\n", DEVICE_ID);
  Serial.printf("Intervalo : %lus\n", INTERVALO_MS / 1000);
  Serial.println("\nIniciando leituras...\n");
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoEnvio >= INTERVALO_MS) {
    ultimoEnvio = agora;

    float temperatura = dht.readTemperature();
    float umidade     = dht.readHumidity();

    if (isnan(temperatura) || isnan(umidade)) {
      Serial.println("[ERRO] Falha ao ler DHT22. Verifique a conexão.");
      return;
    }

    Serial.printf("→ Temperatura: %.1f°C | Umidade: %.1f%%\n", temperatura, umidade);

    // Pisca LED enquanto envia
    digitalWrite(LED_PIN, HIGH);

    bool okTemp = iot.publishHTTP("temperatura", temperatura);
    bool okUmid = iot.publishHTTP("umidade",     umidade);

    digitalWrite(LED_PIN, LOW);

    if (!okTemp || !okUmid) {
      Serial.println("  [AVISO] Envio falhou. Verifique API Key e URL.");
    } else {
      Serial.println("  [OK] Dados enviados com sucesso!");
    }
  }
}
