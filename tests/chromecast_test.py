"""Offline tests: byte ranges, private file access and receiver status isolation."""
import contextlib
import http.client
import importlib.util
import io
import json
from pathlib import Path
import queue
import tempfile
import threading
from types import SimpleNamespace
import unittest

spec = importlib.util.spec_from_file_location("cast_driver", Path(__file__).resolve().parents[1] / "src/chromecast.py")
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class MediaTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / "private video.mp4"
        self.path.write_bytes(b"0123456789")
        self.server = driver.MediaServer(("127.0.0.1", 0))
        self.thread = threading.Thread(target=self.server.serve_forever)
        self.thread.start()
        self.token = self.server.publish(str(self.path))

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.temp.cleanup()

    def request(self, path=None, method="GET", **headers):
        connection = http.client.HTTPConnection(*self.server.server_address)
        connection.request(method, path or self.token, headers=headers)
        response = connection.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        connection.close()
        return result

    def test_only_played_token_accessible(self):
        for path in ("/", "/private video.mp4", "/../etc/passwd", self.token + "?path=/etc/passwd"):
            self.assertEqual(self.request(path.replace(" ", "%20"))[0], 404)
        self.assertEqual(self.request()[2], b"0123456789")
        self.server.publish(str(self.path))
        self.assertEqual(self.request(self.token)[0], 404)

    def test_ranges_and_head(self):
        for value, expected in (("bytes=2-5", b"2345"), ("bytes=8-", b"89"), ("bytes=-3", b"789"), ("bytes=8-99", b"89")):
            status, headers, data = self.request(Range=value)
            self.assertEqual(status, 206)
            self.assertEqual(data, expected)
            self.assertEqual(int(headers["Content-Length"]), len(expected))
        status, headers, data = self.request(method="HEAD")
        self.assertEqual((status, data, headers["Content-Length"]), (200, b"", "10"))
        for invalid in ("bytes=12-", "bytes=5-2", "bytes=-0", "bytes=1-2,4-5", "garbage"):
            self.assertEqual(self.request(Range=invalid)[0], 416)

    def test_stopped_server_revokes_access(self):
        self.server.media = None
        self.assertEqual(self.request()[0], 404)


class AddressTests(unittest.TestCase):
    def test_lan_address_wins_over_vpn(self):
        adapters = [SimpleNamespace(ips=[SimpleNamespace(ip="100.101.109.47", network_prefix=32)]),
                    SimpleNamespace(ips=[SimpleNamespace(ip="10.0.0.100", network_prefix=24)])]
        self.assertEqual(driver.local_address("10.0.0.30", adapters), ("10.0.0.100", True))


class SessionTests(unittest.TestCase):
    def setUp(self):
        self.session = driver.CastSession(None, queue.Queue())
        self.session.item = "load-1"
        self.session.url = "http://local/secret"
        self.session.server = SimpleNamespace(media=True)

    def status(self, state, url=None, reason=None):
        return SimpleNamespace(content_id=url or self.session.url, player_state=state,
                               idle_reason=reason, adjusted_current_time=12, duration=60)

    def test_stale_status_does_not_complete_new_cue(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.session.event("status", self.status("IDLE", "http://local/old", "FINISHED"))
        self.assertEqual(output.getvalue(), "")
        self.assertEqual(self.session.item, "load-1")

    def test_finish_reported_once(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.session.event("status", self.status("PLAYING"))
            finished = self.status("IDLE", reason="FINISHED")
            self.session.event("status", finished)
            self.session.event("status", finished)
        messages = [json.loads(line) for line in output.getvalue().splitlines()]
        self.assertEqual([m["state"] for m in messages], ["PLAYING", "finished"])
        self.assertIsNone(self.session.server.media)

    def test_external_takeover_disconnects(self):
        self.session.seen = True
        with self.assertRaises(ConnectionError):
            self.session.event("status", self.status("PLAYING", "http://other/video"))

    def test_stop_before_launch_never_submits_video(self):
        from unittest.mock import Mock
        controller = Mock()
        controller.status.content_id = None
        self.session.cast = Mock(media_controller=controller)
        self.session.stop()
        self.session.event("launched", ("load-1", {"path": "private.mp4"}, True))
        controller.play_media.assert_not_called()
        self.session.cast.quit_app.assert_not_called()

    def test_stop_after_load_sent_before_status_cancels_receiver(self):
        from unittest.mock import Mock
        controller = Mock()
        controller.status.content_id = "old-content"
        self.session.cast = Mock(media_controller=controller)
        self.session.submitted = True
        self.session.stop()
        self.session.cast.quit_app.assert_called_once_with(timeout=2)
        self.assertIsNone(self.session.item)
        self.assertIsNone(self.session.server.media)

    def test_late_failure_cannot_stop_new_cue(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.session.item = "load-2"
            self.session.event("load_result", ("load-1", False, {"type": "LOAD_FAILED"}))
        self.assertEqual(output.getvalue(), "")
        self.assertEqual(self.session.item, "load-2")

    def test_seek_preserves_pause(self):
        from unittest.mock import Mock
        controller = Mock()
        controller.status.media_session_id = 42
        controller.status.player_state = "PAUSED"
        self.session.cast = Mock(media_controller=controller)
        self.session.command({"op": "seek", "position": 12, "paused": True})
        payload = controller.send_message.call_args.args[0]
        self.assertEqual(payload["resumeState"], "PLAYBACK_PAUSE")
        self.assertEqual(payload["currentTime"], 12)
        self.assertEqual(payload["mediaSessionId"], 42)

    def test_launch_ack_waits_for_media_namespace(self):
        from unittest.mock import Mock
        controller = Mock(is_active=False)
        self.session.cast = Mock(media_controller=controller)
        event = ("load-1", {"path": "demo.mp4"}, True)
        self.session.event("launched", event)
        controller.play_media.assert_not_called()
        self.assertEqual(self.session.pending_launch, event)
        self.session.stop()
        controller.is_active = True
        self.session.event("launched", event)
        controller.play_media.assert_not_called()

    def test_load_command_preserves_start_and_pause(self):
        from unittest.mock import Mock
        controller = Mock()
        self.session.cast = Mock(media_controller=controller)
        self.session.event("launched", ("load-1", {"path": "demo.mp4", "start": 12, "paused": True}, True))
        args, kwargs = controller.play_media.call_args
        self.assertEqual(args, ("http://local/secret", "video/mp4"))
        self.assertEqual(kwargs["current_time"], 12)
        self.assertFalse(kwargs["autoplay"])
        # The callback captures load-1 even after a newer cue is selected.
        self.session.item = "load-2"
        kwargs["callback_function"](False, {"type": "LOAD_FAILED"})
        kind, value = self.session.events.get_nowait()
        self.assertEqual((kind, value[0]), ("load_result", "load-1"))

    def test_connection_loss_does_not_resume_automatically(self):
        with self.assertRaises(ConnectionError):
            self.session.event("lost", None)


if __name__ == "__main__":
    unittest.main()
