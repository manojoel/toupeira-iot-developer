/*
 * 07_GPS_Tracker — Rastreamento GPS via MQTT
 * ─────────────────────────────────────────────────────────────
 * Grupo da plataforma : Localização
 * Protocolo           : MQTT
 * Variáveis enviadas  : latitude (°) · longitude (°) · altitude (m)
 *                       velocidade (km/h) · bateria (%)
 *
 * Hardware necessário:
 *   - ESP32 (qualquer variante)
 *   - Módulo GPS NEO-6M (ou NEO-7M / NEO-8M / NEO-M8N)
 *
 * Ligação GPS NEO-6M → ESP32:
 *
 *   GPS     →   ESP32
 *   VCC     →   3V3
 *   GND     →   GND
 *   TX      →   GPIO 16  (RX2 do ESP32)
 *   RX      →   GPIO 17  (TX2 do ESP32)
 *
 * Bibliotecas necessárias (instale pelo Library Manager):
 *   - ToupeiraIoT
 *   - TinyGPSPlus  (Mikal Hart)
 *   - ArduinoJson
 *   - PubSubClient
 *
 * Como usar:
 *   1. Monte o circuito conforme a ligação acima
 *   2. Na plataforma: crie um dispositivo no grupo "Localização"
 *   3. Cole o DEVICE_ID e a API_KEY abaixo
 *   4. Faça upload e abra o Monitor Serial (115200 baud)
 *   5. Aguarde o GPS adquirir sinal (LED pisca até fixar)
 *   6. Acesse Mapa na plataforma para ver o device em tempo real
 *
 * Tópicos publicados:
 *   iot/{DEVICE_ID}/latitude    →  coordenada decimal (ex: -23.5505)
 *   iot/{DEVICE_ID}/longitude   →  coordenada decimal (ex: -46.6333)
 *   iot/{DEVICE_ID}/altitude    →  metros acima do nível do mar
 *   iot/{DEVICE_ID}/velocidade  →  km/h
 *   iot/{DEVICE_ID}/bateria     →  % (lido via ADC do pino BATERIA_PIN)
 *
 * Nota: o backend atualiza automaticamente a posição do device no Mapa
 * ao receber latitude e longitude.
 */

#include <ToupeiraIoT.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// ── Credenciais do dispositivo ─────────────────────────────────
const char* DEVICE_ID = "COLE_SEU_DEVICE_ID_AQUI";
const char* API_KEY   = "COLE_SUA_API_KEY_AQUI";

// ── Rede WiFi ──────────────────────────────────────────────────
const char* WIFI_SSID = "SeuWiFi";
const char* WIFI_PASS = "SuaSenha";

// ── Broker MQTT (Toupeira IoT Broker) ─────────────────────────
const char*    MQTT_HOST = "192.168.2.114";
const uint16_t MQTT_PORT = 1883;

// ── Hardware ───────────────────────────────────────────────────
#define GPS_RX_PIN   16   // ESP32 recebe do TX do GPS
#define GPS_TX_PIN   17   // ESP32 envia para o RX do GPS
#define GPS_BAUD     9600
#define LED_PIN      2
#define BATERIA_PIN  34   // ADC para leitura de bateria (opcional)

// ── Intervalo de envio ─────────────────────────────────────────
// 5 s para rastreamento em tempo real; aumente para economizar bateria
const unsigned long INTERVALO_MS = 5000;

// ── Variáveis globais ──────────────────────────────────────────
TinyGPSPlus  gps;
HardwareSerial gpsSerial(2);   // UART2 do ESP32
ToupeiraIoT  iot(DEVICE_ID, API_KEY);

unsigned long ultimoEnvio   = 0;
uint32_t      totalEnviados = 0;
bool          gpsFixado     = false;

// ──────────────────────────────────────────────────────────────

// Lê a tensão do pino ADC e estima o percentual de bateria (LiPo 3.7V)
float lerBateria() {
  int raw = analogRead(BATERIA_PIN);
  float tensao = (raw / 4095.0) * 3.3 * 2.0; // divisor de tensão 1:1
  // LiPo: 4.2V = 100%, 3.3V = 0%
  float pct = ((tensao - 3.3) / (4.2 - 3.3)) * 100.0;
  return constrain(pct, 0, 100);
}

// ──────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("\n╔══════════════════════════════════════╗"));
  Serial.println(F("║   Toupeira IoT — GPS Tracker        ║"));
  Serial.println(F("║   Grupo: Localização                 ║"));
  Serial.println(F("╚══════════════════════════════════════╝\n"));

  pinMode(LED_PIN, OUTPUT);

  // Inicializa UART do GPS
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println(F("[GPS]  Aguardando sinal... (pode levar 1-3 min ao ar livre)"));

  // WiFi
  Serial.printf("[WiFi] Conectando a %s ...\n", WIFI_SSID);
  if (!iot.connectWiFi(WIFI_SSID, WIFI_PASS)) {
    Serial.println(F("[ERRO] WiFi falhou."));
    while (true) { digitalWrite(LED_PIN, HIGH); delay(300); digitalWrite(LED_PIN, LOW); delay(300); }
  }
  Serial.println(F("[WiFi] Conectado!"));

  // MQTT
  Serial.printf("[MQTT] Conectando ao broker %s:%d ...\n", MQTT_HOST, MQTT_PORT);
  if (!iot.connectMQTT(MQTT_HOST, MQTT_PORT)) {
    Serial.println(F("[AVISO] MQTT nao conectou. Reconectando no loop..."));
  } else {
    Serial.println(F("[MQTT] Conectado ao Toupeira IoT Broker!"));
  }

  Serial.println();
  Serial.println(F("─── Configuração ───────────────────────"));
  Serial.printf("Device ID : %s\n", DEVICE_ID);
  Serial.printf("Grupo     : Localizacao\n");
  Serial.printf("Broker    : %s:%d\n", MQTT_HOST, MQTT_PORT);
  Serial.printf("Intervalo : %lu s\n", INTERVALO_MS / 1000);
  Serial.println(F("────────────────────────────────────────\n"));
}

// ──────────────────────────────────────────────────────────────

void loop() {
  iot.loop();

  // Alimenta o parser GPS com os bytes recebidos
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  unsigned long agora = millis();
  if (agora - ultimoEnvio < INTERVALO_MS) return;
  ultimoEnvio = agora;

  // Pisca LED enquanto aguarda fix
  if (!gps.location.isValid()) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    Serial.printf("[GPS] Aguardando fix... satellites: %d\n", gps.satellites.value());
    return;
  }

  // GPS com fix — LED aceso fixo
  digitalWrite(LED_PIN, HIGH);

  if (!gpsFixado) {
    gpsFixado = true;
    Serial.println(F("[GPS] Fix adquirido!"));
  }

  double lat  = gps.location.lat();
  double lng  = gps.location.lng();
  double alt  = gps.altitude.isValid()  ? gps.altitude.meters()  : 0;
  double vel  = gps.speed.isValid()     ? gps.speed.kmph()       : 0;
  float  bat  = lerBateria();

  // Monitor Serial
  totalEnviados++;
  Serial.println(F("────────────────────────────────────────"));
  Serial.printf("[#%04u]\n", totalEnviados);
  Serial.printf("  latitude   : %.6f\n", lat);
  Serial.printf("  longitude  : %.6f\n", lng);
  Serial.printf("  altitude   : %.1f m\n", alt);
  Serial.printf("  velocidade : %.1f km/h\n", vel);
  Serial.printf("  bateria    : %.0f %%\n", bat);
  Serial.printf("  satellites : %d\n", gps.satellites.value());

  // Publica no broker — lat/lng atualizam o pino no Mapa da plataforma
  digitalWrite(LED_PIN, LOW);
  bool ok = iot.publish("latitude",   (float)lat)
         && iot.publish("longitude",  (float)lng)
         && iot.publish("altitude",   (float)alt)
         && iot.publish("velocidade", (float)vel)
         && iot.publish("bateria",    bat);
  digitalWrite(LED_PIN, HIGH);

  Serial.println(ok ? F("  [OK] Publicado") : F("  [AVISO] Falha ao publicar"));
}
