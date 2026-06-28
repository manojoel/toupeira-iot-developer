/*
 * 06_DHT11 — Sensor Ambiental com DHT11 via MQTT
 * ─────────────────────────────────────────────────────────────
 * Grupo da plataforma : Sensor Ambiental
 * Protocolo           : MQTT
 * Variáveis enviadas  : temperatura (°C) · umidade (%)
 *
 * Hardware necessário:
 *   - ESP32 (qualquer variante)
 *   - Sensor DHT11 (módulo azul de 3 pinos OU sensor de 4 pinos)
 *
 * Ligação — módulo de 3 pinos (mais comum):
 *
 *   DHT11   →   ESP32
 *   S (sig) →   GPIO 4
 *   + (vcc) →   3V3  (ou 5V)
 *   - (gnd) →   GND
 *
 * Ligação — sensor de 4 pinos (sem módulo):
 *
 *   DHT11   →   ESP32
 *   Pino 1  →   3V3
 *   Pino 2  →   GPIO 4  +  resistor 10kΩ entre pino 2 e 3V3
 *   Pino 3  →   (não conectar — pino NC)
 *   Pino 4  →   GND
 *
 *             ┌──────┐
 *   3V3 ────► │ 1    │
 *   GPIO4 ──► │ 2  ░ │  ← grade frontal (sempre virada para você)
 *   (NC)      │ 3    │
 *   GND ────► │ 4    │
 *             └──────┘
 *
 * Diferenças do DHT11 em relação ao DHT22:
 *   - Precisão menor: valores inteiros (sem casas decimais)
 *   - Faixa limitada: 0–50 °C / 20–90 % UR
 *   - Intervalo mínimo entre leituras: 2 segundos
 *
 * Bibliotecas necessárias (instale pelo Library Manager):
 *   - ToupeiraIoT
 *   - DHT sensor library  (Adafruit)
 *   - Adafruit Unified Sensor
 *   - ArduinoJson
 *   - PubSubClient
 *
 * Como usar:
 *   1. Monte o circuito conforme o diagrama acima
 *   2. Na plataforma: crie um dispositivo no grupo "Sensor Ambiental"
 *   3. Copie o DEVICE_ID e a API_KEY e cole abaixo
 *   4. Informe o IP do broker MQTT em MQTT_HOST
 *   5. Faça upload e abra o Monitor Serial (115200 baud)
 *
 * Tópicos publicados:
 *   iot/{DEVICE_ID}/temperatura   →  valor em °C
 *   iot/{DEVICE_ID}/umidade       →  valor em %
 */

#include <ToupeiraIoT.h>
#include <DHT.h>

// ── Credenciais do dispositivo ─────────────────────────────────
// Obtenha na dashboard: Dispositivos → selecione o device → Chaves de API
const char* DEVICE_ID = "COLE_SEU_DEVICE_ID_AQUI";
const char* API_KEY   = "COLE_SUA_API_KEY_AQUI";

// ── Rede WiFi ──────────────────────────────────────────────────
const char* WIFI_SSID = "SeuWiFi";
const char* WIFI_PASS = "SuaSenha";

// ── Broker MQTT (Toupeira IoT Broker) ─────────────────────────
// IP da máquina onde o backend está rodando.
// O backend imprime os IPs disponíveis no terminal ao iniciar.
// Produção: troque para "broker.toupeira.io"
const char*    MQTT_HOST = "192.168.2.114";
const uint16_t MQTT_PORT = 1883;

// ── Hardware ───────────────────────────────────────────────────
#define DHT_PIN  4       // GPIO conectado ao pino DATA/S do DHT11
#define DHT_TYPE DHT11
#define LED_PIN  2       // LED onboard do ESP32

// ── Intervalo de envio ─────────────────────────────────────────
// DHT11 exige no mínimo 2 s entre leituras — usamos 5 s para segurança
const unsigned long INTERVALO_MS = 5000;

// ── Variáveis globais ──────────────────────────────────────────
DHT         dht(DHT_PIN, DHT_TYPE);
ToupeiraIoT iot(DEVICE_ID, API_KEY);

unsigned long ultimaLeitura      = 0;
uint32_t      totalEnviados      = 0;
uint8_t       errosConsecutivos  = 0;

// ──────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1500); // Aguarda estabilização do DHT11 após energizar

  Serial.println(F("\n╔══════════════════════════════════════╗"));
  Serial.println(F("║   Toupeira IoT — Sensor Ambiental   ║"));
  Serial.println(F("║   DHT11 · temperatura · umidade      ║"));
  Serial.println(F("╚══════════════════════════════════════╝\n"));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  dht.begin();
  Serial.println(F("[DHT11] Sensor iniciado"));

  // ── WiFi ────────────────────────────────────────────────────
  Serial.printf("[WiFi]  Conectando a %s ...\n", WIFI_SSID);
  if (!iot.connectWiFi(WIFI_SSID, WIFI_PASS)) {
    Serial.println(F("[ERRO]  WiFi falhou. Verifique SSID e senha."));
    while (true) {
      digitalWrite(LED_PIN, HIGH); delay(200);
      digitalWrite(LED_PIN, LOW);  delay(200);
    }
  }
  Serial.println(F("[WiFi]  Conectado!"));

  // ── MQTT ─────────────────────────────────────────────────────
  Serial.printf("[MQTT]  Conectando ao broker %s:%d ...\n", MQTT_HOST, MQTT_PORT);
  if (!iot.connectMQTT(MQTT_HOST, MQTT_PORT)) {
    Serial.println(F("[AVISO] MQTT nao conectou agora. Reconectando no loop..."));
  } else {
    Serial.println(F("[MQTT]  Conectado ao Toupeira IoT Broker!"));
  }

  Serial.println();
  Serial.println(F("─── Configuração ───────────────────────"));
  Serial.printf("Device ID : %s\n", DEVICE_ID);
  Serial.printf("Grupo     : Sensor Ambiental\n");
  Serial.printf("Broker    : %s:%d\n", MQTT_HOST, MQTT_PORT);
  Serial.printf("Sensor    : DHT11 no GPIO %d\n", DHT_PIN);
  Serial.printf("Intervalo : %lu s\n", INTERVALO_MS / 1000);
  Serial.println(F("────────────────────────────────────────\n"));

  delay(2000); // DHT11: aguarda 2 s antes da primeira leitura
}

// ──────────────────────────────────────────────────────────────

void loop() {
  iot.loop(); // Mantém conexão MQTT ativa

  unsigned long agora = millis();
  if (agora - ultimaLeitura < INTERVALO_MS) return;
  ultimaLeitura = agora;

  // ── Leitura do DHT11 ────────────────────────────────────────
  float temperatura = dht.readTemperature(); // °C
  float umidade     = dht.readHumidity();    // %

  if (isnan(temperatura) || isnan(umidade)) {
    errosConsecutivos++;
    Serial.printf("[ERRO] Falha na leitura #%d. Verifique a ligacao do DHT11.\n",
                  errosConsecutivos);
    if (errosConsecutivos >= 5) {
      digitalWrite(LED_PIN, HIGH); delay(100);
      digitalWrite(LED_PIN, LOW);
    }
    return;
  }

  errosConsecutivos = 0;

  // ── Monitor Serial ───────────────────────────────────────────
  totalEnviados++;
  Serial.println(F("────────────────────────────────────────"));
  Serial.printf("[#%04u] %02lu:%02lu:%02lu\n",
                totalEnviados,
                (agora / 3600000) % 24,
                (agora / 60000)   % 60,
                (agora / 1000)    % 60);
  Serial.printf("  temperatura : %.0f C\n", temperatura);
  Serial.printf("  umidade     : %.0f %%\n", umidade);

  // ── Publica no broker ─────────────────────────────────────────
  digitalWrite(LED_PIN, HIGH);
  bool okTemp = iot.publish("temperatura", temperatura);
  bool okUmid = iot.publish("umidade",     umidade);
  digitalWrite(LED_PIN, LOW);

  if (okTemp && okUmid) {
    Serial.println(F("  [OK] Publicado no broker"));
  } else {
    Serial.println(F("  [AVISO] Falha ao publicar — reconectando..."));
  }
}
