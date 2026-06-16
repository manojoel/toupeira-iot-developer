"""
04_async_sensor.py — Múltiplos sensores simultâneos (asyncio)

Demonstra como coletar dados de múltiplas fontes ao mesmo tempo
sem bloquear o programa, usando asyncio.

Casos de uso:
  - Raspberry Pi com vários sensores em paralelo
  - Combinação de coleta de dados + servidor web local
  - Alta frequência de envio sem bloquear o event loop

Instalação:
    pip install toupeira-iot[async]
    # ou manualmente:
    pip install aiohttp asyncio-mqtt

Uso:
    python 04_async_sensor.py
"""

import asyncio
import random
import time
import logging
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../.."))
from toupeira_iot import ToupeiraIoTAsync

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")

# ── Configurações ─────────────────────────────────────────────
DEVICE_ID = "COLE_SEU_DEVICE_ID_AQUI"
API_KEY   = "COLE_SUA_API_KEY_AQUI"
API_URL   = "https://api.toupeira.io"

# ── Funções de leitura simuladas ──────────────────────────────
# Substitua por leituras reais dos seus sensores

async def ler_temperatura() -> float:
    await asyncio.sleep(0.1)  # Simula tempo de leitura I2C/1-Wire
    return round(22 + random.uniform(-2, 5), 2)

async def ler_umidade() -> float:
    await asyncio.sleep(0.1)
    return round(60 + random.uniform(-10, 15), 2)

async def ler_luminosidade() -> float:
    await asyncio.sleep(0.05)  # Sensor mais rápido
    return round(random.uniform(0, 1000), 1)  # lux

# ── Tasks assíncronas ─────────────────────────────────────────

async def task_ambiente(iot: ToupeiraIoTAsync):
    """Publica temperatura e umidade a cada 10s."""
    print("[Ambiente] Task iniciada")
    while True:
        temp, umid = await asyncio.gather(ler_temperatura(), ler_umidade())

        await asyncio.gather(
            iot.publish_http("temperatura", temp),
            iot.publish_http("umidade",     umid),
        )
        print(f"[Ambiente] T:{temp}°C | U:{umid}%")
        await asyncio.sleep(10)

async def task_luminosidade(iot: ToupeiraIoTAsync):
    """Publica luminosidade a cada 30s."""
    print("[Luminosidade] Task iniciada")
    while True:
        lux = await ler_luminosidade()
        await iot.publish_http("luminosidade", lux)
        print(f"[Luminosidade] {lux} lux")
        await asyncio.sleep(30)

async def task_heartbeat(iot: ToupeiraIoTAsync):
    """Envia heartbeat a cada 60s."""
    print("[Heartbeat] Task iniciada")
    while True:
        ok = await iot.heartbeat()
        print(f"[Heartbeat] {'OK' if ok else 'FALHOU'}")
        await asyncio.sleep(60)

# ── Entry point ───────────────────────────────────────────────

async def main():
    print("=== Toupeira IoT — AsyncSensor ===")
    print(f"Device: {DEVICE_ID}")
    print(f"API:    {API_URL}\n")

    iot = ToupeiraIoTAsync(DEVICE_ID, API_KEY)
    iot.set_api_url(API_URL)

    # Executa todas as tasks em paralelo
    try:
        await asyncio.gather(
            task_ambiente(iot),
            task_luminosidade(iot),
            task_heartbeat(iot),
        )
    except asyncio.CancelledError:
        pass
    finally:
        await iot.close()
        print("\nEncerrado.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nInterrompido pelo usuário.")
