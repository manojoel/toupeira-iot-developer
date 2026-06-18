/*
 * 06_DHT11 — Temperatura e umidade com DHT11 via MQTT
 * ─────────────────────────────────────────────────────────────
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
 * Diferenças importantes do DHT11 em relação ao DHT22:
 *   - Precisão menor: temperatura em graus inteiros, umidade em inteiros
 *   - Faixa limitada: 0–50 °C / 20–90 % UR
 *   - Intervalo mínimo entre leituras: 2 segundos
 *   - Custo mais baixo — ideal para projetos de aprendizado
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
 *   2. Crie um dispositivo na plataforma Toupeira IoT
 *   3. Copie o Device ID e a API Key e cole nas constantes abaixo
 *   4. Informe o IP da máquina com o backend rodando em MQTT_HOST
 *   5. Faça upload e abra o Monitor Serial (115200 baud)
 *
 * Tópicos publicados:
 *   iot/{DEVICE_ID}/temperatura   →  valor em °C (inteiro)
 *   iot/{DEVICE_ID}/umidade       →  valor em %  (inteiro)
 *   iot/{DEVICE_ID}/indice_calor  →  sensação térmica em °C
 */

#include <ToupeiraIoT.h>
#include <DHT.h>

// ── Credenciais do dispositivo ─────────────────────────────────
// Obtenha esses valores na dashboard: Dispositivos → Chaves de API
const char* DEVICE_ID = "72ed9f9a-af3b-43bd-b043-1083be5256dd";
const char* API_KEY   = "iotk_bea7b9dc252f47b88be758f3f40ae3c8";

// ── Rede WiFi ──────────────────────────────────────────────────
const char* WIFI_SSID = "Energia";
const char* WIFI_PASS = "Energia85*";

// ── Broker MQTT (Toupeira IoT Broker) ─────────────────────────
// Produção:     "broker.toupeira.io"
// Rede local:   IP da máquina onde o backend está rodando
//               Para descobrir: olhe o terminal do backend ao iniciar —
//               ele imprime "MQTT_HOST = 192.168.X.X"
const char*    MQTT_HOST = "192.168.2.114";
const uint16_t MQTT_PORT = 1883;

// ── Pino e tipo do sensor ──────────────────────────────────────
#define DHT_PIN  4       // GPIO conectado ao pino DATA/S do DHT11
#define DHT_TYPE DHT11   // ← DHT11, não DHT22

// ── LED onboard do ESP32 (GPIO 2 na maioria das placas) ────────
#define LED_PIN  2

// ── Intervalo de envio ─────────────────────────────────────────
// DHT11 exige no mínimo 2 s entre leituras.
// Usamos 5 s para margem de segurança.
const unsigned long INTERVALO_MS = 5000;

// ── Variáveis globais ──────────────────────────────────────────
DHT        dht(DHT_PIN, DHT_TYPE);
ToupeiraIoT iot(DEVICE_ID, API_KEY);

unsigned long ultimaLeitura = 0;
uint32_t      totalEnviados  = 0;
uint8_t       errosConsecutivos = 0;

// ──────────────────────────────────────────────────────────────

/*
 * Calcula o índice de calor (sensação térmica) a partir da
 * temperatura e umidade, usando a fórmula de Rothfusz.
 * Retorna NAN se os valores de entrada forem inválidos.
 */
float calcularIndicecalor(float temp, float umidade) {
  if (isnan(temp) || isnan(umidade)) return NAN;

  // Fórmula simplificada válida acima de 27°C
  if (temp < 27.0) return temp;

  float hi = -8.78469475556
    + 1.61139411    * temp
    + 2.33854883889 * umidade
    - 0.14611605    * temp * umidade
    - 0.01230809450 * temp * temp
    - 0.01642482778 * umidade * umidade
    + 0.00221173    * temp * temp * umidade
    + 0.00072546    * temp * umidade * umidade
    - 0.00000358    * temp * temp * umidade * umidade;

  return hi;
}

/*
 * Retorna uma descrição textual da qualidade do ar baseada na umidade.
 * DHT11 mede umidade relativa — útil para avaliar conforto e risco de mofo.
 */
const char* qualidadeAr(float umidade) {
  if (umidade < 30)        return "Seco";
  else if (umidade < 60)   return "Confortavel";
  else if (umidade < 80)   return "Umido";
  else                     return "Muito umido";
}

// ──────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1500); // Aguarda estabilização do DHT11 após energizar

  Serial.println(F("\n╔══════════════════════════════════════╗"));
  Serial.println(F("║   Toupeira IoT — DHT11 + ESP32      ║"));
  Serial.println(F("║   Teste de temperatura e umidade     ║"));
  Serial.println(F("╚══════════════════════════════════════╝\n"));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Inicializa o sensor DHT11
  dht.begin();
  Serial.println(F("[DHT11] Sensor iniciado"));

  // ── Conecta ao WiFi ────────────────────────────────────────
  Serial.printf("[WiFi]  Conectando a %s ...\n", WIFI_SSID);

  if (!iot.connectWiFi(WIFI_SSID, WIFI_PASS)) {
    Serial.println(F("[ERRO]  WiFi falhou. Verifique SSID e senha."));
    // Pisca LED rapidamente para indicar falha
    while (true) {
      digitalWrite(LED_PIN, HIGH); delay(200);
      digitalWrite(LED_PIN, LOW);  delay(200);
    }
  }

  Serial.println(F("[WiFi]  Conectado!"));

  // ── Conecta ao Toupeira IoT Broker ────────────────────────
  Serial.printf("[MQTT]  Conectando ao broker %s:%d ...\n", MQTT_HOST, MQTT_PORT);

  if (!iot.connectMQTT(MQTT_HOST, MQTT_PORT)) {
    // Avisa mas não trava — o loop() tentará reconectar automaticamente
    Serial.println(F("[AVISO] MQTT nao conectou agora. Reconectando no loop..."));
  } else {
    Serial.println(F("[MQTT]  Conectado ao Toupeira IoT Broker!"));
  }

  // ── Resumo da configuração ─────────────────────────────────
  Serial.println();
  Serial.println(F("─── Configuração ───────────────────────"));
  Serial.printf("Device ID : %s\n",      DEVICE_ID);
  Serial.printf("Broker    : %s:%d\n",   MQTT_HOST, MQTT_PORT);
  Serial.printf("Sensor    : DHT11 no GPIO %d\n", DHT_PIN);
  Serial.printf("Intervalo : %lu s\n",   INTERVALO_MS / 1000);
  Serial.println(F("────────────────────────────────────────"));
  Serial.println(F("\nAguardando primeira leitura...\n"));

  // Aguarda 2 s antes da primeira leitura (especificação do DHT11)
  delay(2000);
}

// ──────────────────────────────────────────────────────────────

void loop() {
  // Mantém a conexão MQTT ativa e processa mensagens recebidas
  iot.loop();

  unsigned long agora = millis();

  // Aguarda o intervalo mínimo entre leituras
  if (agora - ultimaLeitura < INTERVALO_MS) return;
  ultimaLeitura = agora;

  // ── Leitura do DHT11 ────────────────────────────────────────
  float temperatura = dht.readTemperature(); // °C
  float umidade     = dht.readHumidity();    // %

  // Valida os dados — DHT11 retorna NAN em caso de erro de comunicação
  if (isnan(temperatura) || isnan(umidade)) {
    errosConsecutivos++;
    Serial.printf("[ERRO] Falha na leitura #%d. Verifique a ligacao do DHT11.\n",
                  errosConsecutivos);

    // Após 5 erros seguidos, pisca LED como alerta
    if (errosConsecutivos >= 5) {
      digitalWrite(LED_PIN, HIGH); delay(100);
      digitalWrite(LED_PIN, LOW);
    }
    return;
  }

  // Leitura OK — reseta contador de erros
  errosConsecutivos = 0;
  float indiceCalor = calcularIndicecalor(temperatura, umidade);

  // ── Exibe no Monitor Serial ──────────────────────────────────
  totalEnviados++;
  Serial.println(F("────────────────────────────────────────"));
  Serial.printf("[#%04u] %02lu:%02lu:%02lu\n",
                totalEnviados,
                (agora / 3600000) % 24,
                (agora / 60000)   % 60,
                (agora / 1000)    % 60);
  Serial.printf("  Temperatura : %.0f °C\n",     temperatura);
  Serial.printf("  Umidade     : %.0f %%\n",     umidade);
  Serial.printf("  Indice calor: %.1f °C\n",     indiceCalor);
  Serial.printf("  Qualidade   : %s\n",          qualidadeAr(umidade));

  // ── Publica no broker via MQTT ───────────────────────────────
  // Acende LED durante a transmissão
  digitalWrite(LED_PIN, HIGH);

  bool okTemp  = iot.publish("temperatura",  temperatura);
  bool okUmid  = iot.publish("umidade",      umidade);
  bool okCalor = iot.publish("indice_calor", indiceCalor);

  digitalWrite(LED_PIN, LOW);

  // Feedback do envio
  if (okTemp && okUmid && okCalor) {
    Serial.println(F("  [OK] Publicado no broker"));
  } else {
    Serial.println(F("  [AVISO] Falha ao publicar — broker desconectado?"));
    Serial.println(F("          Reconectando automaticamente..."));
  }
}
