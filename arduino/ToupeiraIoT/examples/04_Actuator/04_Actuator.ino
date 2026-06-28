/*
 * 04_Actuator — Atuador: Fita LED WS2812B via MQTT
 * ─────────────────────────────────────────────────────────────
 * Grupo da plataforma : Atuador
 * Protocolo           : MQTT
 * Variáveis recebidas : estado (bool) · intensidade (%)
 *
 * Hardware:
 *   ESP32 + Fita LED WS2812B
 *
 *   WS2812B → ESP32
 *   VCC      → 5V externo (use fonte separada para fitas longas)
 *   GND      → GND (GND compartilhado com ESP32)
 *   DIN      → GPIO 5  (com resistor 300–470Ω em série)
 *
 * Bibliotecas:
 *   - ToupeiraIoT
 *   - FastLED
 *   - ArduinoJson
 *   - PubSubClient
 *
 * Como funciona:
 *   A plataforma envia comandos via MQTT no tópico:
 *     actuators/{deviceId}/set
 *
 *   Payload JSON esperado:
 *     {
 *       "power":      true,
 *       "color":      "#FF5500",
 *       "brightness": 80,         (0–100)
 *       "effect":     "solid",    (solid|blink|fade|rainbow|chase|pulse)
 *       "speed":      5           (1–10)
 *     }
 *
 *   Todos os campos são opcionais — somente o que for enviado é alterado.
 */

#include <ToupeiraIoT.h>
#include <FastLED.h>

// ── Credenciais ───────────────────────────────────────────────
const char* DEVICE_ID  = "COLE_SEU_DEVICE_ID_AQUI";
const char* API_KEY    = "COLE_SUA_API_KEY_AQUI";

// ── Rede ──────────────────────────────────────────────────────
const char* WIFI_SSID  = "SeuWiFi";
const char* WIFI_PASS  = "SuaSenha";
const char* MQTT_HOST  = "broker.toupeira.io";

// ── Hardware LED ──────────────────────────────────────────────
#define LED_PIN      5
#define NUM_LEDS     30
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

CRGB leds[NUM_LEDS];

// ── Estado do LED (defaults) ──────────────────────────────────
bool    ledPower   = false;
CRGB    ledColor   = CRGB::Teal;
uint8_t ledBright  = 128;
String  ledEffect  = "solid";
uint8_t ledSpeed   = 5;
uint8_t effectStep = 0;

ToupeiraIoT iot(DEVICE_ID, API_KEY);

// ── Helpers ───────────────────────────────────────────────────

CRGB hexToCRGB(const char* hex) {
  if (!hex || strlen(hex) < 7) return CRGB::Black;
  uint32_t rgb = strtoul(hex + 1, nullptr, 16);
  return CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

// ── Callback: chamado quando a plataforma envia um comando ────

void onCommand(const char* command, JsonObject& params) {
  Serial.println("\n[CMD] Comando recebido:");

  if (params.containsKey("power")) {
    ledPower = params["power"].as<bool>();
    Serial.printf("  power      = %s\n", ledPower ? "true" : "false");
  }
  if (params.containsKey("color")) {
    ledColor = hexToCRGB(params["color"].as<const char*>());
    Serial.printf("  color      = %s\n", params["color"].as<const char*>());
  }
  if (params.containsKey("brightness")) {
    uint8_t pct = params["brightness"].as<uint8_t>();
    ledBright = map(pct, 0, 100, 0, 255);
    FastLED.setBrightness(ledBright);
    Serial.printf("  brightness = %d%%\n", pct);
  }
  if (params.containsKey("effect")) {
    ledEffect = params["effect"].as<String>();
    effectStep = 0;
    Serial.printf("  effect     = %s\n", ledEffect.c_str());
  }
  if (params.containsKey("speed")) {
    ledSpeed = params["speed"].as<uint8_t>();
    Serial.printf("  speed      = %d\n", ledSpeed);
  }
}

// ── Renderização dos efeitos ──────────────────────────────────

void renderLEDs() {
  if (!ledPower) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    return;
  }

  unsigned long now    = millis();
  uint16_t      period = map(ledSpeed, 1, 10, 1000, 60);

  if (ledEffect == "solid") {
    fill_solid(leds, NUM_LEDS, ledColor);
    FastLED.show();

  } else if (ledEffect == "blink") {
    bool on = (now / period) % 2;
    fill_solid(leds, NUM_LEDS, on ? ledColor : CRGB::Black);
    FastLED.show();

  } else if (ledEffect == "fade") {
    uint8_t b = beatsin8(ledSpeed * 4, 10, 255);
    fill_solid(leds, NUM_LEDS, ledColor);
    FastLED.setBrightness(scale8(ledBright, b));
    FastLED.show();
    FastLED.setBrightness(ledBright); // restaura para próximo frame

  } else if (ledEffect == "pulse") {
    uint8_t b = beatsin8(ledSpeed * 2, 0, 255);
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = ledColor;
      leds[i].nscale8(b);
    }
    FastLED.show();

  } else if (ledEffect == "rainbow") {
    static unsigned long lastRainbow = 0;
    if (now - lastRainbow > (unsigned long)(11 - ledSpeed) * 10) {
      lastRainbow = now;
      fill_rainbow(leds, NUM_LEDS, effectStep++, 7);
      FastLED.show();
    }

  } else if (ledEffect == "chase") {
    static unsigned long lastChase = 0;
    if (now - lastChase > period) {
      lastChase = now;
      fadeToBlackBy(leds, NUM_LEDS, 40);
      int pos = effectStep++ % NUM_LEDS;
      leds[pos] = ledColor;
      FastLED.show();
    }
  }
}

// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   Toupeira IoT — Actuator LED      ║");
  Serial.println("╚════════════════════════════════════╝\n");

  // LED desligado durante inicialização
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
         .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(0);
  FastLED.show();

  if (!iot.connectWiFi(WIFI_SSID, WIFI_PASS)) {
    Serial.println("ERRO: WiFi falhou.");
    while (true) delay(1000);
  }

  if (!iot.connectMQTT(MQTT_HOST)) {
    Serial.println("ERRO: MQTT falhou.");
  }

  // Registra o callback de comandos
  iot.onCommand(onCommand);

  // Ajusta brilho inicial
  FastLED.setBrightness(ledBright);

  Serial.println("\nPronto! Aguardando comandos da plataforma...");
  Serial.println("Use a aba 'Atuadores' no portal para controlar a fita.\n");
}

void loop() {
  iot.loop();   // Mantém MQTT ativo e processa mensagens recebidas
  renderLEDs(); // Atualiza a fita com o efeito atual
}
