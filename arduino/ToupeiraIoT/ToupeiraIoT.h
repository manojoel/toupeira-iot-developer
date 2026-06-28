/*
 * ToupeiraIoT.h — SDK oficial para ESP32/Arduino
 * Plataforma Toupeira IoT  |  https://github.com/manojoel/toupeira-iot-developer
 *
 * Dependências (instale via Library Manager):
 *   - ArduinoJson  >= 6.x
 *   - PubSubClient >= 2.8  (apenas se usar MQTT)
 *
 * Início rápido (HTTP):
 *   ToupeiraIoT iot("SEU_DEVICE_ID", "SUA_API_KEY");
 *   iot.connectWiFi("SSID", "SENHA");
 *   iot.setApiUrl("https://api.toupeira.io");
 *   iot.publishHTTP("temperatura", 23.5);
 *
 * Início rápido (MQTT):
 *   ToupeiraIoT iot("SEU_DEVICE_ID", "SUA_API_KEY");
 *   iot.connectWiFi("SSID", "SENHA");
 *   iot.connectMQTT("broker.toupeira.io");
 *   iot.publish("temperatura", 23.5);
 *   iot.loop(); // no loop()
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Callback para comandos recebidos da plataforma (atuadores)
typedef void (*CommandCallback)(const char* command, JsonObject& params);

class ToupeiraIoT {
public:
  ToupeiraIoT(const char* deviceId, const char* apiKey);

  // ── WiFi ───────────────────────────────────────────────────
  bool connectWiFi(const char* ssid, const char* password, uint32_t timeoutMs = 15000);
  bool isWiFiConnected();

  // ── HTTP ───────────────────────────────────────────────────
  void setApiUrl(const char* url);
  bool publishHTTP(const char* metric, float value);
  bool publishHTTP(const char* metric, const char* value);
  bool publishBatchHTTP(JsonDocument& doc);
  bool heartbeat();

  // ── MQTT ───────────────────────────────────────────────────
  // Conecta em um único host (IP ou hostname mDNS, ex: "toupeira-broker.local")
  bool connectMQTT(const char* host, uint16_t port = 1883);
  // Tenta múltiplos hosts em sequência — usa o primeiro que responder
  bool connectMQTT(const char** hosts, uint8_t count, uint16_t port = 1883);
  bool publish(const char* metric, float value);
  bool publish(const char* metric, const char* value);
  void onCommand(CommandCallback callback);
  void loop();
  bool isConnected();

  // ── Utilitários ────────────────────────────────────────────
  String getDeviceId();
  String getMacAddress();

private:
  const char*     _deviceId;
  const char*     _apiKey;
  const char*     _apiUrl    = nullptr;
  const char*     _mqttHost  = nullptr;
  uint16_t        _mqttPort  = 1883;
  // Lista de hosts alternativos (multi-broker fallback)
  const char**    _mqttHosts = nullptr;
  uint8_t         _mqttHostCount = 0;
  uint8_t         _mqttHostIndex = 0; // índice do host ativo
  WiFiClient      _wifiClient;
  PubSubClient    _mqttClient;
  CommandCallback _commandCallback = nullptr;

  bool _reconnectMQTT();
  bool _httpPost(const String& path, const String& body);

  static ToupeiraIoT* _instance;
  static void _mqttCallback(char* topic, byte* payload, unsigned int length);
};
