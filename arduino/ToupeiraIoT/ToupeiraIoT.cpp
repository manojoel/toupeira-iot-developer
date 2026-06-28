#include "ToupeiraIoT.h"

ToupeiraIoT* ToupeiraIoT::_instance = nullptr;

ToupeiraIoT::ToupeiraIoT(const char* deviceId, const char* apiKey)
  : _deviceId(deviceId), _apiKey(apiKey), _mqttClient(_wifiClient) {
  _instance = this;
}

// ── WiFi ─────────────────────────────────────────────────────

bool ToupeiraIoT::connectWiFi(const char* ssid, const char* password, uint32_t timeoutMs) {
  Serial.printf("[Toupeira] Conectando ao WiFi: %s", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println("\n[Toupeira] ERRO: Timeout WiFi!");
      return false;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\n[Toupeira] WiFi OK! IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

bool ToupeiraIoT::isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// ── HTTP ──────────────────────────────────────────────────────

void ToupeiraIoT::setApiUrl(const char* url) {
  _apiUrl = url;
}

bool ToupeiraIoT::_httpPost(const String& path, const String& body) {
  if (!_apiUrl) {
    Serial.println("[Toupeira] ERRO: URL da API não configurada. Chame setApiUrl() primeiro.");
    return false;
  }

  HTTPClient http;
  String url = String(_apiUrl) + path;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", _apiKey);
  http.setTimeout(8000);

  int code = http.POST(body);
  http.end();

  return code == 200 || code == 202 || code == 204;
}

bool ToupeiraIoT::publishHTTP(const char* metric, float value) {
  StaticJsonDocument<128> doc;
  doc["deviceId"] = _deviceId;
  doc["metric"]   = metric;
  doc["value"]    = value;

  String body;
  serializeJson(doc, body);

  bool ok = _httpPost("/telemetry", body);
  if (ok) Serial.printf("[Toupeira] HTTP OK  %s = %.4f\n", metric, value);
  else    Serial.printf("[Toupeira] HTTP FAIL %s = %.4f\n", metric, value);
  return ok;
}

bool ToupeiraIoT::publishHTTP(const char* metric, const char* value) {
  StaticJsonDocument<128> doc;
  doc["deviceId"] = _deviceId;
  doc["metric"]   = metric;
  doc["value"]    = value;

  String body;
  serializeJson(doc, body);

  return _httpPost("/telemetry", body);
}

bool ToupeiraIoT::publishBatchHTTP(JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  return _httpPost("/telemetry/batch", body);
}

bool ToupeiraIoT::heartbeat() {
  return _httpPost("/devices/" + String(_deviceId) + "/heartbeat", "{}");
}

// ── MQTT ──────────────────────────────────────────────────────

bool ToupeiraIoT::connectMQTT(const char* host, uint16_t port) {
  _mqttPort = port;
  _mqttClient.setCallback(_mqttCallback);
  _mqttClient.setBufferSize(512);

  // Resolve hostname mDNS (.local) para IP antes de conectar
  String hostStr(host);
  if (hostStr.endsWith(".local")) {
    Serial.printf("[Toupeira] Resolvendo %s via mDNS...\n", host);
    if (!MDNS.begin("esp32")) {
      Serial.println("[Toupeira] mDNS: falha ao iniciar");
    }
    IPAddress ip = MDNS.queryHost(hostStr.substring(0, hostStr.length() - 6).c_str(), 3000);
    if (ip != INADDR_NONE) {
      Serial.printf("[Toupeira] %s → %s\n", host, ip.toString().c_str());
      _mqttClient.setServer(ip, _mqttPort);
    } else {
      Serial.printf("[Toupeira] mDNS: não resolveu %s — tentando direto\n", host);
      _mqttClient.setServer(host, _mqttPort);
    }
  } else {
    _mqttClient.setServer(host, _mqttPort);
  }

  _mqttHost = host;
  return _reconnectMQTT();
}

bool ToupeiraIoT::connectMQTT(const char** hosts, uint8_t count, uint16_t port) {
  // Tenta cada host em sequência até um conectar
  _mqttHosts     = hosts;
  _mqttHostCount = count;
  _mqttPort      = port;
  _mqttClient.setCallback(_mqttCallback);
  _mqttClient.setBufferSize(512);

  for (uint8_t i = 0; i < count; i++) {
    Serial.printf("[Toupeira] Tentando broker %s:%d...\n", hosts[i], port);
    _mqttHost      = hosts[i];
    _mqttHostIndex = i;
    _mqttClient.setServer(_mqttHost, _mqttPort);

    String clientId = "toupeira-" + String(_deviceId).substring(0, 8)
                    + "-" + String(random(0xffff), HEX);

    if (_mqttClient.connect(clientId.c_str(), _apiKey, _apiKey)) {
      Serial.printf("[Toupeira] MQTT OK! Conectado em %s\n", _mqttHost);
      if (_commandCallback) {
        char topic[128];
        snprintf(topic, sizeof(topic), "actuators/%s/set", _deviceId);
        _mqttClient.subscribe(topic);
      }
      return true;
    }
    Serial.printf("[Toupeira] %s falhou (rc=%d)\n", hosts[i], _mqttClient.state());
    delay(1000);
  }

  Serial.println("[Toupeira] ERRO: nenhum broker respondeu.");
  return false;
}

bool ToupeiraIoT::_reconnectMQTT() {
  if (!_mqttHost) return false;

  uint8_t attempts = 0;
  while (!_mqttClient.connected() && attempts < 3) {
    String clientId = "toupeira-" + String(_deviceId).substring(0, 8)
                    + "-" + String(random(0xffff), HEX);

    Serial.printf("[Toupeira] MQTT conectando... (tentativa %d)\n", attempts + 1);

    if (_mqttClient.connect(clientId.c_str(), _apiKey, _apiKey)) {
      Serial.println("[Toupeira] MQTT OK!");

      if (_commandCallback) {
        char topic[128];
        snprintf(topic, sizeof(topic), "actuators/%s/set", _deviceId);
        _mqttClient.subscribe(topic);
        Serial.printf("[Toupeira] Inscrito em: %s\n", topic);
      }
      return true;
    }

    Serial.printf("[Toupeira] MQTT falhou (rc=%d). Aguardando...\n", _mqttClient.state());
    delay(3000);
    attempts++;
  }

  Serial.println("[Toupeira] ERRO: Não foi possível conectar ao broker MQTT.");
  return false;
}

bool ToupeiraIoT::publish(const char* metric, float value) {
  if (!_mqttClient.connected() && !_reconnectMQTT()) return false;

  char topic[128];
  snprintf(topic, sizeof(topic), "iot/%s/%s", _deviceId, metric);

  StaticJsonDocument<64> doc;
  doc["value"] = value;
  char payload[64];
  serializeJson(doc, payload);

  bool ok = _mqttClient.publish(topic, payload, /*retain=*/false);
  if (ok) Serial.printf("[Toupeira] MQTT OK  %s = %.4f\n", metric, value);
  return ok;
}

bool ToupeiraIoT::publish(const char* metric, const char* value) {
  if (!_mqttClient.connected() && !_reconnectMQTT()) return false;

  char topic[128];
  snprintf(topic, sizeof(topic), "iot/%s/%s", _deviceId, metric);

  StaticJsonDocument<64> doc;
  doc["value"] = value;
  char payload[64];
  serializeJson(doc, payload);

  return _mqttClient.publish(topic, payload);
}

void ToupeiraIoT::onCommand(CommandCallback callback) {
  _commandCallback = callback;

  if (_mqttClient.connected()) {
    char topic[128];
    snprintf(topic, sizeof(topic), "actuators/%s/set", _deviceId);
    _mqttClient.subscribe(topic);
    Serial.printf("[Toupeira] Aguardando comandos em: %s\n", topic);
  }
}

void ToupeiraIoT::loop() {
  if (!_mqttClient.connected()) _reconnectMQTT();
  _mqttClient.loop();
}

bool ToupeiraIoT::isConnected() {
  return _mqttClient.connected();
}

// ── Callback MQTT (estático) ──────────────────────────────────

void ToupeiraIoT::_mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (!_instance || !_instance->_commandCallback) return;

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.printf("[Toupeira] MQTT: JSON inválido (%s)\n", err.c_str());
    return;
  }

  // Extrai o sufixo do tópico como nome do comando
  String topicStr(topic);
  int lastSlash = topicStr.lastIndexOf('/');
  String command = lastSlash >= 0 ? topicStr.substring(lastSlash + 1) : topicStr;

  JsonObject params = doc.as<JsonObject>();
  _instance->_commandCallback(command.c_str(), params);
}

// ── Utilitários ───────────────────────────────────────────────

String ToupeiraIoT::getDeviceId() {
  return String(_deviceId);
}

String ToupeiraIoT::getMacAddress() {
  return WiFi.macAddress();
}
