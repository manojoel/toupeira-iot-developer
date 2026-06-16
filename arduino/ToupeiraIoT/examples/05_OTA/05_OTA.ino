/*
 * 05_OTA — Sensor + Atualização de Firmware via Rede (OTA)
 * ─────────────────────────────────────────────────────────────
 * Hardware:
 *   ESP32 + DHT22 (mesmo circuito do exemplo 01)
 *
 * Bibliotecas:
 *   - ToupeiraIoT
 *   - DHT sensor library  (Adafruit)
 *   - Adafruit Unified Sensor
 *   - ArduinoJson
 *   - ArduinoOTA (já inclusa no pacote ESP32 para Arduino IDE)
 *
 * O que é OTA?
 *   Permite atualizar o firmware do ESP32 via WiFi, sem precisar
 *   conectar o cabo USB. Após o primeiro upload com este sketch,
 *   as próximas atualizações podem ser feitas wirelessly.
 *
 * Como usar OTA no Arduino IDE:
 *   1. Faça o primeiro upload via USB com este sketch
 *   2. Vá em: Ferramentas → Porta
 *   3. Selecione a porta de rede "toupeira-XXXXXXXX em X.X.X.X"
 *   4. Próximos uploads serão via WiFi (peça a senha OTA_PASS)
 *
 * Como usar OTA via script Python (esp32_ota.py do repositório):
 *   python3 esp32_ota.py --host 192.168.1.X --file firmware.bin
 *
 * IMPORTANTE:
 *   Não esqueça de chamar ArduinoOTA.handle() no loop().
 *   Qualquer delay longo (> 1s) pode interromper a atualização.
 */

#include <ToupeiraIoT.h>
#include <ArduinoOTA.h>
#include <DHT.h>

// ── Credenciais ───────────────────────────────────────────────
const char* DEVICE_ID  = "COLE_SEU_DEVICE_ID_AQUI";
const char* API_KEY    = "COLE_SUA_API_KEY_AQUI";

// ── Rede ──────────────────────────────────────────────────────
const char* WIFI_SSID  = "SeuWiFi";
const char* WIFI_PASS  = "SuaSenha";
const char* API_URL    = "https://api.toupeira.io";

// ── OTA ───────────────────────────────────────────────────────
// Senha necessária para autenticar uploads OTA
const char* OTA_PASS   = "toupeira123";

// ── Hardware ──────────────────────────────────────────────────
#define DHT_PIN  4
#define DHT_TYPE DHT22
#define LED_PIN  2

DHT dht(DHT_PIN, DHT_TYPE);
ToupeiraIoT iot(DEVICE_ID, API_KEY);

const unsigned long INTERVALO_MS   = 10000;
const unsigned long HEARTBEAT_MS   = 60000; // Heartbeat a cada 1 min
unsigned long ultimoEnvio          = 0;
unsigned long ultimoHeartbeat      = 0;

// ── Configuração OTA ─────────────────────────────────────────

void setupOTA() {
  // Nome visível na lista de portas do Arduino IDE
  String hostname = "toupeira-" + String(DEVICE_ID).substring(0, 8);
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.setPassword(OTA_PASS);

  ArduinoOTA.onStart([]() {
    String tipo = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
    Serial.printf("\n[OTA] Iniciando atualização de %s...\n", tipo.c_str());
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Concluído! Reiniciando...");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static uint8_t lastPct = 0;
    uint8_t pct = progress / (total / 100);
    if (pct != lastPct) {
      lastPct = pct;
      Serial.printf("[OTA] %d%%\r", pct);
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    const char* msg = "Desconhecido";
    switch (error) {
      case OTA_AUTH_ERROR:    msg = "Autenticação falhou"; break;
      case OTA_BEGIN_ERROR:   msg = "Início falhou";       break;
      case OTA_CONNECT_ERROR: msg = "Conexão falhou";      break;
      case OTA_RECEIVE_ERROR: msg = "Recebimento falhou";  break;
      case OTA_END_ERROR:     msg = "Fim falhou";          break;
    }
    Serial.printf("[OTA] ERRO: %s\n", msg);
  });

  ArduinoOTA.begin();

  Serial.printf("[OTA] Pronto! Hostname: %s\n", ArduinoOTA.getHostname().c_str());
  Serial.printf("[OTA] IP: %s\n", WiFi.localIP().toString().c_str());
}

// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   Toupeira IoT — OTA + Sensor      ║");
  Serial.println("╚════════════════════════════════════╝\n");

  pinMode(LED_PIN, OUTPUT);
  dht.begin();

  if (!iot.connectWiFi(WIFI_SSID, WIFI_PASS)) {
    Serial.println("ERRO: WiFi falhou.");
    while (true) delay(1000);
  }

  iot.setApiUrl(API_URL);
  setupOTA();

  Serial.println("\nPronto! Enviando dados e aguardando OTA...\n");
}

void loop() {
  // OTA DEVE ser a primeira chamada do loop
  ArduinoOTA.handle();

  unsigned long agora = millis();

  // Envio de dados do sensor
  if (agora - ultimoEnvio >= INTERVALO_MS) {
    ultimoEnvio = agora;

    float temperatura = dht.readTemperature();
    float umidade     = dht.readHumidity();

    if (!isnan(temperatura) && !isnan(umidade)) {
      Serial.printf("→ T:%.1f°C | U:%.1f%%\n", temperatura, umidade);

      digitalWrite(LED_PIN, HIGH);
      iot.publishHTTP("temperatura", temperatura);
      iot.publishHTTP("umidade",     umidade);
      digitalWrite(LED_PIN, LOW);
    }
  }

  // Heartbeat periódico (mantém dispositivo como "online" na plataforma)
  if (agora - ultimoHeartbeat >= HEARTBEAT_MS) {
    ultimoHeartbeat = agora;
    iot.heartbeat();
    Serial.println("[HB] Heartbeat enviado");
  }
}
