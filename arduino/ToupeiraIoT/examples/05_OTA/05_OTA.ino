/*
 * 05_OTA — Update de Firmware via Plataforma Toupeira IoT
 * ─────────────────────────────────────────────────────────────
 * Hardware:
 *   ESP32 (qualquer variante)
 *
 * Bibliotecas:
 *   - ToupeiraIoT
 *   - ArduinoJson
 *   - PubSubClient
 *
 * Como funciona:
 *   1. Na plataforma, vá em "Firmware OTA" e faça upload do .bin
 *   2. Clique em "Deploy" — a plataforma publica no tópico MQTT:
 *        ota/{deviceId}/update
 *      com o payload: { firmwareId, url, version, size, checksum }
 *   3. Este sketch recebe o comando, baixa o .bin via HTTP e
 *      instala o firmware automaticamente
 *   4. Após reiniciar, o device reporta sucesso/falha de volta
 *
 * Como gerar o .bin no Arduino IDE:
 *   Sketch → Exportar Binário Compilado
 *   (o arquivo .bin fica na pasta do sketch)
 *
 * IMPORTANTE:
 *   - O OTA_ROLLBACK_TIMEOUT define quanto tempo o device espera
 *     para confirmar a instalação antes de reverter para o firmware anterior
 *   - Nunca bloqueie o loop() por mais de ~1s (use millis())
 */

#include <ToupeiraIoT.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>

// ── Credenciais ───────────────────────────────────────────────
const char* DEVICE_ID  = "COLE_SEU_DEVICE_ID_AQUI";
const char* API_KEY    = "COLE_SUA_API_KEY_AQUI";

// ── Rede ──────────────────────────────────────────────────────
const char* WIFI_SSID  = "SeuWiFi";
const char* WIFI_PASS  = "SuaSenha";
const char* MQTT_HOST  = "broker.toupeira.io";
const char* API_URL    = "https://api.toupeira.io";

// ── LED de status (onboard) ───────────────────────────────────
#define LED_PIN  2

ToupeiraIoT iot(DEVICE_ID, API_KEY);

// Estado do OTA
bool otaPending  = false;
String otaUrl    = "";
String otaVersion = "";
String otaFirmwareId = "";

// ── Realiza o download e flash do firmware ────────────────────

bool performOTA(const String& url, const String& firmwareId) {
  Serial.printf("\n[OTA] Iniciando update: %s\n", url.c_str());
  Serial.println("[OTA] Baixando firmware...");

  HTTPClient http;
  http.begin(url);
  http.setTimeout(30000);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[OTA] ERRO HTTP: %d\n", code);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("[OTA] ERRO: tamanho de arquivo inválido");
    http.end();
    return false;
  }

  Serial.printf("[OTA] Tamanho: %d bytes\n", contentLength);

  if (!Update.begin(contentLength)) {
    Serial.printf("[OTA] ERRO ao iniciar update: %s\n", Update.errorString());
    http.end();
    return false;
  }

  WiFiClient* stream  = http.getStreamPtr();
  size_t      written = 0;
  uint8_t     buf[1024];
  int         lastPct = -1;

  while (http.connected() && written < (size_t)contentLength) {
    size_t available = stream->available();
    if (available) {
      size_t toRead = min(available, sizeof(buf));
      size_t r      = stream->readBytes(buf, toRead);
      written       += Update.write(buf, r);

      int pct = (written * 100) / contentLength;
      if (pct != lastPct) {
        lastPct = pct;
        Serial.printf("[OTA] Progresso: %d%%\r", pct);
        digitalWrite(LED_PIN, (pct / 10) % 2); // pisca durante download
      }
    }
    delay(1);
  }

  http.end();
  Serial.println();

  if (!Update.end(true)) {
    Serial.printf("[OTA] ERRO ao finalizar: %s\n", Update.errorString());
    return false;
  }

  Serial.println("[OTA] Download e flash concluídos!");
  return true;
}

// ── Reporta resultado para a plataforma ──────────────────────

void reportStatus(const String& firmwareId, bool success) {
  iot.set_api_url(API_URL);

  HTTPClient http;
  String url = String(API_URL) + "/firmware/" + DEVICE_ID + "/" + firmwareId + "/status";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", API_KEY);

  String body = success
    ? "{\"status\":\"installed\"}"
    : "{\"status\":\"failed\",\"error\":\"Flash falhou\"}";

  int code = http.POST(body);
  http.end();

  Serial.printf("[OTA] Status reportado: %s (HTTP %d)\n",
                success ? "installed" : "failed", code);
}

// ── Callback MQTT: recebe comando da plataforma ───────────────

void onOtaCommand(const char* command, JsonObject& params) {
  if (!params.containsKey("url")) return;

  otaUrl        = params["url"].as<String>();
  otaVersion    = params["version"].as<String>();
  otaFirmwareId = params["firmwareId"].as<String>();

  Serial.printf("\n[OTA] Comando recebido: v%s\n", otaVersion.c_str());
  Serial.printf("[OTA] URL: %s\n", otaUrl.c_str());
  Serial.printf("[OTA] FirmwareID: %s\n", otaFirmwareId.c_str());

  otaPending = true;
}

// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   Toupeira IoT — Firmware OTA      ║");
  Serial.printf( "║   Versão atual: %s%-19s║\n", "compilado em ", __DATE__);
  Serial.println("╚════════════════════════════════════╝\n");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if (!iot.connectWiFi(WIFI_SSID, WIFI_PASS)) {
    Serial.println("ERRO: WiFi falhou.");
    while (true) delay(1000);
  }

  iot.set_api_url(API_URL);
  iot.connectMQTT(MQTT_HOST);
  iot.onCommand(onOtaCommand);  // registra callback para ota/{deviceId}/set

  Serial.println("\nPronto! Aguardando comandos de firmware da plataforma...\n");
  Serial.printf("Tópico MQTT: ota/%s/update\n\n", DEVICE_ID);
}

void loop() {
  iot.loop(); // mantém MQTT ativo

  // Processa OTA fora do callback MQTT (evita timeout do broker)
  if (otaPending) {
    otaPending = false;

    Serial.println("[OTA] Processando update...");
    digitalWrite(LED_PIN, HIGH);

    bool ok = performOTA(otaUrl, otaFirmwareId);
    reportStatus(otaFirmwareId, ok);

    if (ok) {
      Serial.println("[OTA] Reiniciando em 3 segundos...");
      delay(3000);
      ESP.restart();
    } else {
      Serial.println("[OTA] Update falhou. Continuando com firmware atual.");
      digitalWrite(LED_PIN, LOW);
    }
  }
}
