"""Beam's optional Cast driver. stdin/stdout: newline-delimited JSON.

Only an explicitly played file is served, behind an unguessable URL. No directory
listing, arbitrary paths, cue metadata, or controller screen is exposed.
"""
import json
import ipaddress
import mimetypes
import os
import queue
import re
import secrets
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from uuid import UUID


def emit(event, **values):
    print(json.dumps(dict(event=event, **values)), flush=True)


def local_address(host, adapters):
    """Prefer an on-link LAN address over a VPN's advertised source address."""
    target = ipaddress.ip_address(host)
    candidates = []
    for adapter in adapters:
        for address in adapter.ips:
            if not isinstance(address.ip, str):
                continue  # IPv6 addresses use tuples in ifaddr.
            interface = ipaddress.ip_interface(f"{address.ip}/{address.network_prefix}")
            if not interface.is_loopback and target in interface.network:
                candidates.append((interface.network.prefixlen, address.ip))
    if candidates:
        return max(candidates, key=lambda entry: entry[0])[1], True
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
        route.connect((host, 8009))
        return route.getsockname()[0], False


class MediaServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, on_link=False, interface=None):
        super().__init__(address, MediaHandler, bind_and_activate=False)
        self.media = None
        self.on_link = on_link
        try:
            if interface:
                # Bind before listen, so SYN-ACKs also use the LAN interface.
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE,
                                       interface.encode() + b"\0")
            if on_link:
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_DONTROUTE, 1)
            self.server_bind()
            self.server_activate()
        except Exception:
            self.server_close()
            raise

    def get_request(self):
        client, address = super().get_request()
        if self.on_link:
            # Replies to a local receiver must remain on-link even if a VPN
            # installs an overlapping subnet route. No system routes change.
            client.setsockopt(socket.SOL_SOCKET, socket.SO_DONTROUTE, 1)
        return client, address

    def publish(self, path):
        # Open before publishing; reject missing/unreadable files immediately.
        with open(path, "rb"):
            pass
        token = "/" + secrets.token_urlsafe(32)
        self.media = (token, path)
        return token


class MediaHandler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_HEAD(self):
        self.serve(False)

    def do_GET(self):
        self.serve(True)

    def serve(self, body):
        media = self.server.media
        if not media or self.path != media[0]:
            self.send_error(404)
            return
        try:
            with open(media[1], "rb") as source:
                size = os.fstat(source.fileno()).st_size
                start, end = 0, size - 1
                partial = self.headers.get("Range")
                if partial:
                    match = re.fullmatch(r"bytes=(\d*)-(\d*)", partial)
                    if not match or not any(match.groups()):
                        self.send_error(416)
                        return
                    a, b = match.groups()
                    if a:
                        start = int(a)
                        end = min(int(b), end) if b else end
                    else:
                        start = max(0, size - int(b))
                    if start > end or start >= size:
                        self.send_response(416)
                        self.send_header("Content-Range", f"bytes */{size}")
                        self.end_headers()
                        return
                self.send_response(206 if partial else 200)
                self.send_header("Content-Type", mimetypes.guess_type(media[1])[0] or "application/octet-stream")
                self.send_header("Content-Length", str(max(0, end - start + 1)))
                self.send_header("Accept-Ranges", "bytes")
                self.send_header("Access-Control-Allow-Origin", "*")
                if partial:
                    self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
                self.end_headers()
                if body:
                    source.seek(start)
                    remaining = end - start + 1
                    while remaining > 0:
                        data = source.read(min(remaining, 256 * 1024))
                        if not data:
                            break
                        self.wfile.write(data)
                        remaining -= len(data)
        except (OSError, ConnectionError):
            # Receiver may abandon a range when seeking or switching cues.
            return


class CastSession:
    def __init__(self, library, events):
        self.library, self.events = library, events
        self.cast = self.browser = self.server = None
        self.url = self.item = None
        self.loading = 0
        self.seen = False
        self.submitted = False
        self.pending_launch = None

    def new_media_status(self, status):
        self.events.put(("status", status))

    def load_media_failed(self, *_args):
        # This legacy callback has no Cast request identity. The per-LOAD
        # callback below handles failures, including delayed ones, safely.
        pass

    def new_connection_status(self, status):
        if status.status in ("DISCONNECTED", "LOST", "FAILED"):
            self.events.put(("lost", None))

    def command(self, request):
        op = request["op"]
        if op == "discover":
            zconf = self.library.create_zeroconf()
            browser = self.library.CastBrowser(self.library.SimpleCastListener(lambda *_: None), zconf)
            browser.start_discovery()
            try:
                time.sleep(8)  # Discovery is isolated from the Qt UI thread.
                devices = [self.library.get_cast_type(d, zconf) if d.cast_type is None else d
                           for d in list(browser.devices.values())]
                emit("devices", devices=[dict(id=str(d.uuid), label=d.friendly_name or str(d.uuid))
                                         for d in devices if d.cast_type == "cast"])
            finally:
                browser.stop_discovery()
                zconf.close()
        elif op == "connect":
            casts, self.browser = self.library.get_listed_chromecasts(
                uuids=[UUID(request["id"])], discovery_timeout=8, tries=1, timeout=5)
            if not casts:
                raise RuntimeError("Receiver not found. Search again on the same local network.")
            self.cast = casts[0]
            self.cast.wait(timeout=10)
            self.cast.media_controller.register_status_listener(self)
            self.cast.register_connection_listener(self)
            import ifaddr  # Installed as a PyChromecast dependency.
            adapters = ifaddr.get_adapters()
            address, on_link = local_address(self.cast.cast_info.host, adapters)
            interface = next((adapter.name for adapter in adapters for ip in adapter.ips
                              if ip.ip == address), None) if on_link else None
            self.server = MediaServer((address, int(os.environ.get("BEAM_CAST_PORT", "45000"))),
                                      on_link=on_link, interface=interface)
            threading.Thread(target=self.server.serve_forever, daemon=True).start()
            emit("connected")
        elif op == "play":
            self.item = request["id"]
            token = self.server.publish(request["path"])
            host, port = self.server.server_address
            self.url = f"http://{host}:{port}{token}"
            self.seen = False
            self.loading = time.monotonic()
            self.submitted = False
            item = self.item
            # Launch first, then submit LOAD on our main loop only if this cue
            # is still wanted. A late launch callback cannot undo Stop.
            self.cast.media_controller.launch(callback_function=lambda success, response:
                self.events.put(("launched", (item, request, success))))
        elif op == "stop":
            self.stop()
        elif op == "pause":
            (self.cast.media_controller.pause if request["paused"] else self.cast.media_controller.play)()
        elif op == "seek":
            controller = self.cast.media_controller
            # PyChromecast's seek() always resumes playback. Beam must preserve
            # a paused audience frame while changing its position.
            controller.send_message({"type": "SEEK",
                "mediaSessionId": controller.status.media_session_id,
                "currentTime": request["position"],
                "resumeState": "PLAYBACK_PAUSE" if request.get("paused", controller.status.player_state == "PAUSED") else "PLAYBACK_START"},
                inc_session_id=True)
        elif op == "volume":
            self.cast.set_volume(request["volume"] / 100)
            self.cast.set_volume_muted(request["muted"])
        else:
            raise ValueError("Unknown output command")

    def stop(self):
        url, submitted = self.url, self.submitted
        self.url = self.item = None
        self.pending_launch = None
        self.submitted = False
        self.loading = 0
        if self.server:
            self.server.media = None
        if self.cast and url:
            if self.cast.media_controller.status.content_id == url:
                self.cast.media_controller.stop(timeout=2)
            elif submitted:
                # LOAD was sent, but no status has arrived yet. Closing our
                # receiver app cancels it even without a media session ID.
                self.cast.quit_app(timeout=2)

    def event(self, kind, value):
        if kind == "lost":
            raise ConnectionError("Chromecast disconnected. Reconnect explicitly to resume.")
        if kind == "launched":
            item, request, success = value
            if item != self.item:
                return
            if not success:
                self.event("load_result", (item, False, None))
                return
            if not self.cast.media_controller.is_active:
                # Some receivers acknowledge LAUNCH before advertising media
                # namespaces. Wait rather than launching a second time.
                self.pending_launch = value
                return
            self.pending_launch = None
            self.submitted = True
            self.cast.media_controller.play_media(
                self.url, mimetypes.guess_type(request["path"])[0] or "video/mp4",
                title=request.get("title", "Beam"), current_time=request.get("start", 0),
                autoplay=not request.get("paused", False), stream_type="BUFFERED",
                callback_function=lambda ok, response:
                    self.events.put(("load_result", (item, ok, response))))
            return
        if kind == "load_result":
            item, success, response = value
            if item != self.item:
                return
            if not success or (response or {}).get("type") in ("LOAD_FAILED", "LOAD_CANCELLED", "INVALID_REQUEST"):
                emit("error", message="Chromecast could not load this video. Check its format and network access.")
                emit("status", id=self.item, state="error")
                self.stop()
            return
        if kind != "status" or not self.item:
            return
        if value.content_id != self.url:
            if self.seen:
                raise ConnectionError("The receiver switched to other content. Reconnect to resume Beam.")
            return
        state = value.player_state
        if state in ("PLAYING", "PAUSED", "BUFFERING"):
            self.seen = True
            self.loading = 0
        if state == "IDLE":
            if not self.seen and value.idle_reason != "ERROR":
                return
            state = "finished" if value.idle_reason == "FINISHED" else "error" if value.idle_reason == "ERROR" else "stopped"
        emit("status", id=self.item, state=state,
             position=value.adjusted_current_time or 0, duration=value.duration or 0)
        if state in ("finished", "stopped", "error"):
            if state == "error":
                emit("error", message="The receiver rejected this video. Try an MP4 with H.264 video and AAC audio.")
            self.item = self.url = None
            self.submitted = False
            self.server.media = None
            self.loading = 0

    def close(self):
        try:
            self.stop()
        except Exception:
            pass
        if self.server:
            self.server.shutdown()
            self.server.server_close()
        if self.cast:
            self.cast.disconnect(timeout=1)
        if self.browser:
            self.browser.stop_discovery()


def main():
    try:
        import pychromecast
    except ImportError:
        emit("error", message="Chromecast support is missing. Run bin/setup-chromecast from the Beam source folder.")
        return 1
    events = queue.Queue()

    def read_input():
        for line in sys.stdin:
            try:
                events.put(("command", json.loads(line)))
            except ValueError:
                pass
        events.put(("command", {"op": "disconnect"}))

    threading.Thread(target=read_input, daemon=True).start()
    session = CastSession(pychromecast, events)
    tick = 0
    try:
        while True:
            try:
                kind, value = events.get(timeout=0.25)
            except queue.Empty:
                kind, value = "tick", None
            if kind == "command":
                if value.get("op") == "disconnect":
                    break
                try:
                    session.command(value)
                except Exception as error:
                    emit("error", message=str(error))
                    if value.get("op") == "connect":
                        break
                    if value.get("op") == "play":
                        emit("status", id=value.get("id"), state="error")
                        session.stop()
            else:
                session.event(kind, value)
            if session.pending_launch and session.cast.media_controller.is_active:
                session.event("launched", session.pending_launch)
            now = time.monotonic()
            if session.loading and now - session.loading > 20:
                emit("error", message="Video loading timed out. Check the firewall and receiver format support.")
                emit("status", id=session.item, state="error")
                session.stop()
            if session.item and session.submitted and session.cast.media_controller.is_active and now - tick > 1:
                tick = now
                session.cast.media_controller.update_status()
    except Exception as error:
        emit("error", message=str(error))
    finally:
        session.close()
        emit("disconnected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
