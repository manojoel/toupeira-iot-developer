"""
ToupeiraIoT — SDK Python (síncrono)

Instalação:
    pip install toupeira-iot          # quando publicado no PyPI
    pip install -e ./python           # instalação local

Dependências:
    pip install requests paho-mqtt

Uso rápido (HTTP):
    from toupeira_iot import ToupeiraIoT

    iot = ToupeiraIoT("SEU_DEVICE_ID", "SUA_API_KEY")
    iot.set_api_url("https://api.toupeira.io")
    iot.publish_http("temperatura", 23.5)

Uso rápido (MQTT):
    iot = ToupeiraIoT("SEU_DEVICE_ID", "SUA_API_KEY")
    iot.connect_mqtt("broker.toupeira.io")
    iot.publish("temperatura", 23.5)
    iot.loop_forever()
"""

import json
import logging
import time
import uuid
from typing import Callable, Dict, Optional, Union

logger = logging.getLogger("toupeira_iot")

try:
    import requests as _requests
    _HTTP = True
except ImportError:
    _HTTP = False

try:
    import paho.mqtt.client as _mqtt
    _MQTT = True
except ImportError:
    _MQTT = False


class ToupeiraIoT:
    """Cliente principal da plataforma Toupeira IoT (síncrono)."""

    def __init__(self, device_id: str, api_key: str):
        """
        Args:
            device_id: UUID do dispositivo (obtido na plataforma).
            api_key:   API Key do dispositivo (obtida na plataforma).
        """
        self.device_id = device_id
        self.api_key   = api_key

        self._api_url: Optional[str] = None
        self._mqtt_host: Optional[str] = None
        self._mqtt_port: int = 1883
        self._mqtt_client = None
        self._command_callback: Optional[Callable] = None
        self._connected = False

    # ── HTTP ──────────────────────────────────────────────────

    def set_api_url(self, url: str) -> None:
        """Define a URL base da API REST.

        Args:
            url: Ex: "https://api.toupeira.io" ou "http://localhost:3000"
        """
        self._api_url = url.rstrip("/")

    def publish_http(self, metric: str, value: Union[float, str]) -> bool:
        """Envia uma leitura via HTTP POST /telemetry.

        Args:
            metric: Nome da métrica (ex: "temperatura").
            value:  Valor numérico ou string.

        Returns:
            True se enviado com sucesso.
        """
        if not _HTTP:
            raise ImportError("Execute: pip install requests")
        if not self._api_url:
            raise RuntimeError("Chame set_api_url() antes de publish_http().")

        try:
            resp = _requests.post(
                f"{self._api_url}/telemetry",
                json={"deviceId": self.device_id, "metric": metric, "value": value},
                headers={"X-Device-Key": self.api_key},
                timeout=10,
            )
            ok = resp.status_code in (200, 202)
            if ok:
                logger.debug("HTTP OK  %s = %s", metric, value)
            else:
                logger.warning("HTTP %d  %s = %s", resp.status_code, metric, value)
            return ok
        except Exception as exc:
            logger.error("HTTP erro: %s", exc)
            return False

    def publish_batch(self, readings: Dict[str, Union[float, str]]) -> bool:
        """Envia múltiplas métricas em uma única requisição.

        Args:
            readings: Dicionário {métrica: valor}.

        Returns:
            True se todas foram aceitas.
        """
        if not _HTTP:
            raise ImportError("Execute: pip install requests")
        if not self._api_url:
            raise RuntimeError("Chame set_api_url() antes de publish_batch().")

        payload = [
            {"deviceId": self.device_id, "metric": k, "value": v}
            for k, v in readings.items()
        ]
        try:
            resp = _requests.post(
                f"{self._api_url}/telemetry/batch",
                json=payload,
                headers={"X-Device-Key": self.api_key},
                timeout=10,
            )
            return resp.status_code in (200, 202)
        except Exception as exc:
            logger.error("HTTP batch erro: %s", exc)
            return False

    def heartbeat(self) -> bool:
        """Notifica a plataforma que o dispositivo está online."""
        if not _HTTP or not self._api_url:
            return False
        try:
            resp = _requests.post(
                f"{self._api_url}/devices/{self.device_id}/heartbeat",
                headers={"X-Device-Key": self.api_key},
                timeout=5,
            )
            return resp.status_code in (200, 204)
        except Exception:
            return False

    # ── MQTT ──────────────────────────────────────────────────

    def connect_mqtt(self, host: str, port: int = 1883) -> None:
        """Conecta ao broker MQTT.

        Args:
            host: Endereço do broker (ex: "broker.toupeira.io").
            port: Porta TCP (padrão 1883).
        """
        if not _MQTT:
            raise ImportError("Execute: pip install paho-mqtt")

        self._mqtt_host = host
        self._mqtt_port = port

        client_id = f"toupeira-py-{self.device_id[:8]}-{uuid.uuid4().hex[:6]}"
        self._mqtt_client = _mqtt.Client(client_id=client_id)
        self._mqtt_client.username_pw_set(self.api_key, self.api_key)
        self._mqtt_client.on_connect    = self._on_connect
        self._mqtt_client.on_disconnect = self._on_disconnect
        self._mqtt_client.on_message    = self._on_message

        logger.info("Conectando ao broker %s:%d ...", host, port)
        self._mqtt_client.connect(host, port, keepalive=60)
        self._mqtt_client.loop_start()

        # Aguarda conexão (máx 10s)
        deadline = time.time() + 10
        while not self._connected and time.time() < deadline:
            time.sleep(0.1)

        if not self._connected:
            raise ConnectionError(f"Não foi possível conectar ao broker {host}:{port}")

    def publish(self, metric: str, value: Union[float, str]) -> bool:
        """Publica uma leitura via MQTT.

        Args:
            metric: Nome da métrica.
            value:  Valor numérico ou string.

        Returns:
            True se enfileirado com sucesso.
        """
        if not self._mqtt_client:
            raise RuntimeError("Chame connect_mqtt() primeiro.")

        topic   = f"iot/{self.device_id}/{metric}"
        payload = json.dumps({"value": value})
        result  = self._mqtt_client.publish(topic, payload, qos=1)

        ok = result.rc == 0
        if ok:
            logger.debug("MQTT OK  %s = %s", metric, value)
        else:
            logger.warning("MQTT FAIL  %s = %s  rc=%d", metric, value, result.rc)
        return ok

    def on_command(self, callback: Callable[[str, dict], None]) -> None:
        """Registra callback para receber comandos da plataforma (atuadores).

        O callback recebe (command: str, params: dict).

        Exemplo:
            def handle(command, params):
                print(params.get("color"), params.get("brightness"))

            iot.on_command(handle)
        """
        self._command_callback = callback

        if self._mqtt_client and self._connected:
            topic = f"actuators/{self.device_id}/set"
            self._mqtt_client.subscribe(topic, qos=1)
            logger.info("Aguardando comandos em: %s", topic)

    def loop_forever(self) -> None:
        """Bloqueia a execução mantendo a conexão MQTT ativa.
        Interrompa com Ctrl+C.
        """
        if not self._mqtt_client:
            raise RuntimeError("Chame connect_mqtt() primeiro.")
        try:
            logger.info("Loop ativo. Pressione Ctrl+C para sair.")
            self._mqtt_client.loop_forever()
        except KeyboardInterrupt:
            logger.info("Interrompido.")
        finally:
            self.disconnect()

    def disconnect(self) -> None:
        """Desconecta do broker MQTT."""
        if self._mqtt_client:
            self._mqtt_client.loop_stop()
            self._mqtt_client.disconnect()
            self._connected = False
            logger.info("Desconectado.")

    def is_connected(self) -> bool:
        """Retorna True se conectado ao broker MQTT."""
        return self._connected

    # ── Callbacks internos ────────────────────────────────────

    def _on_connect(self, client, userdata, flags, rc):
        codes = {
            0: "Conectado",
            1: "Versão de protocolo incorreta",
            2: "Client ID inválido",
            3: "Broker indisponível",
            4: "Credenciais incorretas",
            5: "Não autorizado",
        }
        if rc == 0:
            self._connected = True
            logger.info("MQTT conectado ao broker %s:%d", self._mqtt_host, self._mqtt_port)

            if self._command_callback:
                topic = f"actuators/{self.device_id}/set"
                client.subscribe(topic, qos=1)
                logger.info("Inscrito em: %s", topic)
        else:
            logger.error("MQTT falhou: %s (rc=%d)", codes.get(rc, "Erro desconhecido"), rc)

    def _on_disconnect(self, client, userdata, rc):
        self._connected = False
        if rc != 0:
            logger.warning("MQTT desconectado inesperadamente (rc=%d). Reconectando...", rc)

    def _on_message(self, client, userdata, msg):
        if not self._command_callback:
            return
        try:
            data    = json.loads(msg.payload.decode())
            topic   = msg.topic
            command = topic.split("/")[-1]  # "set", "get", etc.
            self._command_callback(command, data)
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            logger.error("Mensagem inválida: %s", exc)
