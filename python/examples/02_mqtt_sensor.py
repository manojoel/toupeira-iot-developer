"""
02_mqtt_sensor.py — Publicação contínua via MQTT

Conecta ao broker MQTT e publica métricas em loop.
Reconexão automática em caso de queda.

Instalação:
    pip install toupeira-iot

Uso:
    python 02_mqtt_sensor.py
"""

import time
import random
import logging
import signal
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../.."))
from toupeira_iot import ToupeiraIoT

# ── Configurações ─────────────────────────────────────────────
DEVICE_ID  = "COLE_SEU_DEVICE_ID_AQUI"
API_KEY    = "COLE_SUA_API_KEY_AQUI"
MQTT_HOST  = "broker.toupeira.io"
MQTT_PORT  = 1883
INTERVALO_S = 5

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

# ── Inicialização ─────────────────────────────────────────────
iot = ToupeiraIoT(DEVICE_ID, API_KEY)

# ── Encerramento gracioso com Ctrl+C ──────────────────────────
def encerrar(sig, frame):
    print("\n\nEncerrando conexão MQTT...")
    iot.disconnect()
    sys.exit(0)

signal.signal(signal.SIGINT, encerrar)
signal.signal(signal.SIGTERM, encerrar)

# ── Sensor simulado ───────────────────────────────────────────
# Substitua esta função pela leitura real (DHT22, BME280, etc.)

_temp_base = 22.0
_umid_base = 60.0

def ler_sensores() -> dict:
    global _temp_base, _umid_base
    _temp_base += random.uniform(-0.3, 0.3)
    _umid_base += random.uniform(-0.5, 0.5)
    _temp_base = max(15.0, min(40.0, _temp_base))
    _umid_base = max(30.0, min(90.0, _umid_base))
    return {
        "temperatura": round(_temp_base, 2),
        "umidade":     round(_umid_base, 2),
    }

# ── Conexão e loop ────────────────────────────────────────────

print("=== Toupeira IoT — MQTTSensor ===")
print(f"Broker: {MQTT_HOST}:{MQTT_PORT}")
print(f"Tópico: iot/{DEVICE_ID}/{{métrica}}")
print(f"Intervalo: {INTERVALO_S}s\n")

iot.connect_mqtt(MQTT_HOST, MQTT_PORT)

contador = 0
print("Conectado! Publicando dados...\n")

while True:
    leituras = ler_sensores()
    contador += 1

    for metrica, valor in leituras.items():
        iot.publish(metrica, valor)

    print(f"[#{contador:04d}] " + " | ".join(f"{k}: {v}" for k, v in leituras.items()))

    time.sleep(INTERVALO_S)
