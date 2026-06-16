"""
03_raspberry_dht.py — DHT22 no Raspberry Pi via MQTT

Lê temperatura e umidade de um sensor DHT22 conectado ao
GPIO do Raspberry Pi e envia para a plataforma via MQTT.

Hardware:
    Raspberry Pi (qualquer modelo) + Sensor DHT22

    DHT22   → Raspberry Pi
    VCC     → 3.3V (pino 1)
    GND     → GND  (pino 6)
    DATA    → GPIO 4 (pino 7) + resistor 10kΩ para 3.3V

Instalação:
    pip install toupeira-iot Adafruit_DHT

    # Se Adafruit_DHT não funcionar no seu modelo de Pi:
    pip install adafruit-circuitpython-dht
    sudo apt install libgpiod2

Uso:
    python 03_raspberry_dht.py
"""

import time
import logging
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../.."))
from toupeira_iot import ToupeiraIoT

# ── Configurações ─────────────────────────────────────────────
DEVICE_ID   = "COLE_SEU_DEVICE_ID_AQUI"
API_KEY     = "COLE_SUA_API_KEY_AQUI"
MQTT_HOST   = "broker.toupeira.io"

DHT_GPIO    = 4      # Número do pino GPIO (BCM)
INTERVALO_S = 10     # DHT22 suporta no máximo 1 leitura a cada 2s

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")

# ── Inicialização do sensor ───────────────────────────────────

def criar_sensor():
    """Tenta importar Adafruit_DHT (legado) ou CircuitPython."""
    try:
        import Adafruit_DHT
        sensor = Adafruit_DHT.DHT22
        def ler():
            umidade, temperatura = Adafruit_DHT.read_retry(sensor, DHT_GPIO)
            return temperatura, umidade
        print("[Sensor] Adafruit_DHT inicializado (GPIO BCM %d)" % DHT_GPIO)
        return ler

    except ImportError:
        pass

    try:
        import board
        import adafruit_dht
        dht = adafruit_dht.DHT22(getattr(board, f"D{DHT_GPIO}"))
        def ler():
            return dht.temperature, dht.humidity
        print("[Sensor] adafruit-circuitpython-dht inicializado (GPIO %d)" % DHT_GPIO)
        return ler

    except (ImportError, AttributeError):
        pass

    # Fallback: valores simulados (útil para testar sem hardware)
    import random
    print("[Sensor] AVISO: biblioteca DHT não encontrada. Usando valores simulados.")
    def ler():
        return round(20 + random.uniform(0, 10), 1), round(50 + random.uniform(0, 20), 1)
    return ler


ler_dht = criar_sensor()

# ── Conexão à plataforma ──────────────────────────────────────

print("\n=== Toupeira IoT — Raspberry Pi + DHT22 ===")

iot = ToupeiraIoT(DEVICE_ID, API_KEY)
iot.connect_mqtt(MQTT_HOST)

print("Conectado! Iniciando leituras...\n")

erros_consecutivos = 0
contador = 0

try:
    while True:
        try:
            temperatura, umidade = ler_dht()

            if temperatura is None or umidade is None:
                erros_consecutivos += 1
                if erros_consecutivos >= 5:
                    print("[AVISO] 5 falhas consecutivas. Verifique o sensor.")
                    erros_consecutivos = 0
                time.sleep(2)
                continue

            erros_consecutivos = 0
            contador += 1

            iot.publish("temperatura", round(temperatura, 2))
            iot.publish("umidade",     round(umidade, 2))

            print(f"[#{contador:04d}] T:{temperatura:.1f}°C | U:{umidade:.1f}%")

        except RuntimeError as exc:
            # DHT22 às vezes falha em leituras individuais — normal
            print(f"[AVISO] Erro de leitura: {exc}")

        time.sleep(INTERVALO_S)

except KeyboardInterrupt:
    print("\nEncerrado.")
    iot.disconnect()
