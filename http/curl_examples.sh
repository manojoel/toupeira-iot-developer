#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Toupeira IoT — Exemplos de integração via curl
#
# Substitua as variáveis abaixo com os dados do seu dispositivo.
# Todas as rotas autenticadas aceitam o header:  X-Device-Key: SUA_API_KEY
#
# Uso:
#   chmod +x curl_examples.sh
#   ./curl_examples.sh
# ─────────────────────────────────────────────────────────────────────────────

BASE_URL="https://api.toupeira.io"   # ou http://localhost:3000
DEVICE_ID="COLE_SEU_DEVICE_ID_AQUI"
API_KEY="COLE_SUA_API_KEY_AQUI"
USER_TOKEN="SEU_JWT_TOKEN_AQUI"       # obtido após login

# ─────────────────────────────────────────────────────────────────────────────
# AUTENTICAÇÃO
# ─────────────────────────────────────────────────────────────────────────────

echo "── Login ────────────────────────────────────────────────"
curl -s -X POST "$BASE_URL/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"email":"seu@email.com","password":"sua_senha"}' | jq .

echo ""
echo "── Criar conta ──────────────────────────────────────────"
curl -s -X POST "$BASE_URL/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"name":"Meu Nome","email":"seu@email.com","password":"sua_senha"}' | jq .

# ─────────────────────────────────────────────────────────────────────────────
# DISPOSITIVOS
# ─────────────────────────────────────────────────────────────────────────────

echo ""
echo "── Listar dispositivos ──────────────────────────────────"
curl -s -X GET "$BASE_URL/devices" \
  -H "Authorization: Bearer $USER_TOKEN" | jq .

echo ""
echo "── Criar dispositivo ────────────────────────────────────"
curl -s -X POST "$BASE_URL/devices" \
  -H "Authorization: Bearer $USER_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"ESP32 Sala","type":"sensor","description":"Sensor de ambiente"}' | jq .

echo ""
echo "── Detalhe do dispositivo ───────────────────────────────"
curl -s -X GET "$BASE_URL/devices/$DEVICE_ID" \
  -H "Authorization: Bearer $USER_TOKEN" | jq .

# ─────────────────────────────────────────────────────────────────────────────
# TELEMETRIA — ENVIO (autenticado com API Key do dispositivo)
# ─────────────────────────────────────────────────────────────────────────────

echo ""
echo "── Enviar leitura única ─────────────────────────────────"
curl -s -X POST "$BASE_URL/telemetry" \
  -H "X-Device-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d "{\"deviceId\":\"$DEVICE_ID\",\"metric\":\"temperatura\",\"value\":23.5}" | jq .

echo ""
echo "── Enviar múltiplas leituras (batch) ────────────────────"
curl -s -X POST "$BASE_URL/telemetry/batch" \
  -H "X-Device-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d "[
    {\"deviceId\":\"$DEVICE_ID\",\"metric\":\"temperatura\",\"value\":23.5},
    {\"deviceId\":\"$DEVICE_ID\",\"metric\":\"umidade\",\"value\":68.2},
    {\"deviceId\":\"$DEVICE_ID\",\"metric\":\"pressao\",\"value\":1013.25}
  ]" | jq .

echo ""
echo "── Heartbeat (dispositivo online) ───────────────────────"
curl -s -X POST "$BASE_URL/devices/$DEVICE_ID/heartbeat" \
  -H "X-Device-Key: $API_KEY" | jq .

# ─────────────────────────────────────────────────────────────────────────────
# TELEMETRIA — CONSULTA (autenticado com JWT do usuário)
# ─────────────────────────────────────────────────────────────────────────────

echo ""
echo "── Consultar telemetria (últimas 1h) ────────────────────"
curl -s -X GET "$BASE_URL/telemetry/$DEVICE_ID?range=1h" \
  -H "Authorization: Bearer $USER_TOKEN" | jq .

echo ""
echo "── Exportar CSV ─────────────────────────────────────────"
curl -s -X GET "$BASE_URL/telemetry/$DEVICE_ID/export?range=24h" \
  -H "Authorization: Bearer $USER_TOKEN" \
  -o "telemetria_${DEVICE_ID}.csv"
echo "CSV salvo em: telemetria_${DEVICE_ID}.csv"

# ─────────────────────────────────────────────────────────────────────────────
# ALERTAS
# ─────────────────────────────────────────────────────────────────────────────

echo ""
echo "── Criar regra de alerta ────────────────────────────────"
curl -s -X POST "$BASE_URL/alerts/rules" \
  -H "Authorization: Bearer $USER_TOKEN" \
  -H "Content-Type: application/json" \
  -d "{
    \"deviceId\":  \"$DEVICE_ID\",
    \"metric\":    \"temperatura\",
    \"operator\":  \">\",
    \"threshold\": 35,
    \"channels\":  {\"email\": true, \"webhook\": null}
  }" | jq .

echo ""
echo "── Listar regras de alerta ──────────────────────────────"
curl -s -X GET "$BASE_URL/alerts/rules" \
  -H "Authorization: Bearer $USER_TOKEN" | jq .

echo ""
echo "── Histórico de alertas disparados ──────────────────────"
curl -s -X GET "$BASE_URL/alerts/history?deviceId=$DEVICE_ID" \
  -H "Authorization: Bearer $USER_TOKEN" | jq .

# ─────────────────────────────────────────────────────────────────────────────
# GERENCIAMENTO DE API KEYS
# ─────────────────────────────────────────────────────────────────────────────

echo ""
echo "── Listar API Keys do dispositivo ───────────────────────"
curl -s -X GET "$BASE_URL/devices/$DEVICE_ID/keys" \
  -H "Authorization: Bearer $USER_TOKEN" | jq .

echo ""
echo "── Gerar nova API Key ───────────────────────────────────"
curl -s -X POST "$BASE_URL/devices/$DEVICE_ID/keys" \
  -H "Authorization: Bearer $USER_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"label":"Produção"}' | jq .

echo ""
echo "── Plano e uso de armazenamento ─────────────────────────"
curl -s -X GET "$BASE_URL/users/me" \
  -H "Authorization: Bearer $USER_TOKEN" | jq '{plan, storageUsedMB, planLimits}'

echo ""
echo "Concluído!"
