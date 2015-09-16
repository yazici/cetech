import enet
import json
import threading


class ConsoleProxy(threading.Thread):
    def __init__(self, address, port):
        super(ConsoleProxy, self).__init__()

        host = enet.Host(None, 1, 1, 0, 0)
        self.address = enet.Address(address, port)
        self.host = host
        self.peer = self.host.connect(self.address, 0)

        self.connect = False
        self.disconnecting = False

        self.handlers = {}

        self.on_log = []

    def register_handler(self, type, handler):
        if type not in self.handlers:
            self.handlers[type] = []

        self.handlers[type].append(handler)

    def disconnect(self):
        self.disconnecting = True

    def run(self):
        while not self.disconnecting:
            self._tick()

        self._safe_disconnect()

    def _safe_disconnect(self):
        if self.connect:
            self.peer.disconnect(0)
            while True:
                event = self.host.service(0)
                if event.type == enet.EVENT_TYPE_DISCONNECT:
                    self.connect = False
                    self.disconnecting = False
                    break

    def tick(self):
        if not self.disconnecting:
            self._tick()
        else:
            self._safe_disconnect()

        return True

    def _tick(self):
        event = self.host.service(0)
        if event.type == enet.EVENT_TYPE_CONNECT:
            self.connect = True

        elif event.type == enet.EVENT_TYPE_DISCONNECT:
            self.connect = False

        elif event.type == enet.EVENT_TYPE_RECEIVE:
            message = json.loads(event.packet.data.decode("utf-8"))
            type = message["type"]
            data = message["data"]

            self._call_handler(type, data)

            if type == 'log':
                for on_log in self.on_log:
                    on_log(data["level"], data["where"], data['msg'])

    def _call_handler(self, type, data):
        if type not in self.handlers:
            return

        handlers = self.handlers[type]
        for h in handlers:
            h(**data)

    def send_command(self, cmd_name, **kwargs):
        if not self.connect:
            return

        command_json = {
            "name": cmd_name,
            "args": kwargs
        }

        dump = str.encode('%s\0' % json.dumps(command_json))
        packet = enet.Packet(dump, enet.PACKET_FLAG_RELIABLE)

        self.peer.send(0, packet)
        self.host.flush()
