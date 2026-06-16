"""
01_basic_http.py — Envio de dados via HTTP

O exemplo mais simples: lê temperatura da CPU do sistema como
métrica de teste e envia para a plataforma via HTTP POST.
Funciona em qualquer computador com Python 3.8+.

Instalação:
    pip install toupeira-iot psutil

Uso:
    python 01_basic_http.py
"""

import time
import logging
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../.."))
from toupeira_iot import ToupeiraIoT

# ── Configurações ─────────────────────────────────────────────
DEVICE_ID = "COLE_SEU_DEVICE_ID_AQUI"
API_KEY   = "COLE_SUA_API_KEY_AQUI"
API_URL   = "https://api.toupeira.io"  # ou http://localhost:3000

INTERVALO_S = 10  # Envia a cada 10 segundos

# ── Setup ─────────────────────────────────────────────────────
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")

iot = ToupeiraIoT(DEVICE_ID, API_KEY)
iot.set_api_url(API_URL)

# ── Leitura simulada (substitua pela leitura real do seu sensor)

def ler_sensores() -> dict:
    """Retorna leituras dos sensores. Adapte para seu hardware."""
    try:
        import psutil
        cpu_temp = None
        temps = psutil.sensors_temperatures()
        if temps:
            for name, entries in temps.items():
                if entries:
                    cpu_temp = entries[0].current
                    break
        return {
            "cpu_temperatura": cpu_temp or 40.0,
            "cpu_uso":         psutil.cpu_percent(interval=1),
            "ram_uso":         psutil.virtual_memory().percent,
        }
    except ImportError:
        # Sem psutil: retorna valores simulados
        import random
        return {
            "temperatura": round(20 + random.uniform(0, 10), 2),
            "umidade":     round(50 + random.uniform(0, 20), 2),
        }

# ── Loop principal ────────────────────────────────────────────

print("=== Toupeira IoT — BasicHTTP ===")
print(f"Device: {DEVICE_ID}")
print(f"API:    {API_URL}")
print(f"Intervalo: {INTERVALO_S}s\n")
print("Pressione Ctrl+C para parar.\n")

contador = 0
try:
    while True:
        leituras = ler_sensores()
        contador += 1

        print(f"[#{contador:04d}] Enviando: {leituras}")

        # Envia em batch (uma única requisição HTTP)
        ok = iot.publish_batch(leituras)

        if ok:
            print(f"       ✓ Enviado com sucesso\n")
        else:
            print(f"       ✗ Falha no envio. Verifique API Key e URL.\n")

        time.sleep(INTERVALO_S)

except KeyboardInterrupt:
    print("\nEncerrado.")
