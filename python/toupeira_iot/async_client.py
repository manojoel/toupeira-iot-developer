"""
ToupeiraIoTAsync — cliente assíncrono (asyncio)

Ideal para:
  - Raspberry Pi com múltiplos sensores simultâneos
  - Scripts que combinam coleta de dados e servidor web
  - Alta frequência de envio sem bloquear o event loop

Dependências:
    pip install aiohttp asyncio-mqtt

Uso:
    import asyncio
    from toupeira_iot import ToupeiraIoTAsync

    async def main():
        iot = ToupeiraIoTAsync("SEU_DEVICE_ID", "SUA_API_KEY")
        iot.set_api_url("https://api.toupeira.io")
        await iot.publish_http("temperatura", 23.5)

    asyncio.run(main())
"""

import json
import logging
from typing import Callable, Dict, Optional, Union

logger = logging.getLogger("toupeira_iot.async")

try:
    import aiohttp as _aiohttp
    _AIOHTTP = True
except ImportError:
    _AIOHTTP = False

try:
    from asyncio_mqtt import Client as _MqttClient
    _ASYNC_MQTT = True
except ImportError:
    _ASYNC_MQTT = False


class ToupeiraIoTAsync:
    """Cliente assíncrono da plataforma Toupeira IoT (asyncio)."""

    def __init__(self, device_id: str, api_key: str):
        self.device_id = device_id
        self.api_key   = api_key
        self._api_url: Optional[str] = None
        self._session: Optional[object] = None

    def set_api_url(self, url: str) -> None:
        self._api_url = url.rstrip("/")

    async def _get_session(self):
        if not _AIOHTTP:
            raise ImportError("Execute: pip install aiohttp")
        if self._session is None or self._session.closed:  # type: ignore[attr-defined]
            self._session = _aiohttp.ClientSession(
                headers={"X-Device-Key": self.api_key}
            )
        return self._session

    async def publish_http(self, metric: str, value: Union[float, str]) -> bool:
        """Envia uma leitura via HTTP POST assíncrono."""
        if not self._api_url:
            raise RuntimeError("Chame set_api_url() antes de publish_http().")

        session = await self._get_session()
        try:
            async with session.post(  # type: ignore[union-attr]
                f"{self._api_url}/telemetry",
                json={"deviceId": self.device_id, "metric": metric, "value": value},
                timeout=_aiohttp.ClientTimeout(total=10),
            ) as resp:
                ok = resp.status in (200, 202)
                if ok:
                    logger.debug("HTTP OK  %s = %s", metric, value)
                else:
                    logger.warning("HTTP %d  %s", resp.status, metric)
                return ok
        except Exception as exc:
            logger.error("HTTP erro: %s", exc)
            return False

    async def publish_batch(self, readings: Dict[str, Union[float, str]]) -> bool:
        """Envia múltiplas métricas em uma requisição."""
        if not self._api_url:
            raise RuntimeError("Chame set_api_url() antes de publish_batch().")

        payload = [
            {"deviceId": self.device_id, "metric": k, "value": v}
            for k, v in readings.items()
        ]
        session = await self._get_session()
        try:
            async with session.post(  # type: ignore[union-attr]
                f"{self._api_url}/telemetry/batch",
                json=payload,
                timeout=_aiohttp.ClientTimeout(total=10),
            ) as resp:
                return resp.status in (200, 202)
        except Exception as exc:
            logger.error("HTTP batch erro: %s", exc)
            return False

    async def heartbeat(self) -> bool:
        """Notifica a plataforma que o dispositivo está online."""
        if not self._api_url:
            return False
        session = await self._get_session()
        try:
            async with session.post(  # type: ignore[union-attr]
                f"{self._api_url}/devices/{self.device_id}/heartbeat",
                timeout=_aiohttp.ClientTimeout(total=5),
            ) as resp:
                return resp.status in (200, 204)
        except Exception:
            return False

    async def mqtt_publish_loop(
        self,
        host: str,
        readings_fn: Callable[[], Dict[str, float]],
        interval_s: float = 5.0,
        port: int = 1883,
    ) -> None:
        """Loop assíncrono MQTT: chama readings_fn() e publica periodicamente.

        Args:
            host:        Endereço do broker.
            readings_fn: Função que retorna dict {métrica: valor}.
            interval_s:  Intervalo entre leituras (segundos).
            port:        Porta TCP do broker.
        """
        if not _ASYNC_MQTT:
            raise ImportError("Execute: pip install asyncio-mqtt")

        import asyncio

        async with _MqttClient(
            host,
            port=port,
            username=self.api_key,
            password=self.api_key,
        ) as client:
            logger.info("MQTT assíncrono conectado a %s:%d", host, port)
            try:
                while True:
                    readings = readings_fn()
                    for metric, value in readings.items():
                        topic   = f"iot/{self.device_id}/{metric}"
                        payload = json.dumps({"value": value})
                        await client.publish(topic, payload, qos=1)
                        logger.debug("MQTT OK  %s = %s", metric, value)
                    await asyncio.sleep(interval_s)
            except asyncio.CancelledError:
                logger.info("Loop MQTT encerrado.")

    async def close(self) -> None:
        """Fecha a sessão HTTP."""
        if self._session and not self._session.closed:  # type: ignore[attr-defined]
            await self._session.close()  # type: ignore[union-attr]
