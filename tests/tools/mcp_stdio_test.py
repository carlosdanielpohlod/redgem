"""Exercise the real stdio server, including native stdout isolation and saved scene edits."""

import base64
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

PROGRAM, GAME = map(Path, sys.argv[1:3])
del sys.argv[1:3]


def request(number, method, params=None):
    return {"jsonrpc": "2.0", "id": number, "method": method, "params": params or {}}


def modern_call(number, name, args):
    params = {"name": name, "arguments": args, "_meta": {
        "io.modelcontextprotocol/protocolVersion": "2026-07-28",
        "io.modelcontextprotocol/clientCapabilities": {},
    }}
    return request(number, "tools/call", params)


PNG_SIGNATURE = b"\x89PNG"


def exchange(game, messages, *flags, env=None):
    result = subprocess.run([str(PROGRAM), "mcp", str(game), *flags],
                            input="".join(json.dumps(message) + "\n" for message in messages),
                            text=True, capture_output=True, timeout=60, env=env)
    if result.returncode:
        raise AssertionError(result.stderr)
    return [json.loads(line) for line in result.stdout.splitlines()], result.stderr


class McpStdioTest(unittest.TestCase):
    def test_legacy_handshake_and_native_logs_preserve_protocol_replies(self):
        messages = [request(1, "initialize", {
            "protocolVersion": "2025-11-25", "capabilities": {},
            "clientInfo": {"name": "protocol-test", "version": "1"},
        }), {"jsonrpc": "2.0", "method": "notifications/initialized"},
            request(2, "tools/call", {"name": "game_run", "arguments": {
                "frames": 1, "print": "begin Debug.log('protocol-noise'); 123 end",
            }}), request(3, "tools/call", {"name": "events", "arguments": {}})]
        replies, _ = exchange(GAME, messages, "--allow", "game_run,events")
        self.assertEqual([reply["id"] for reply in replies], [1, 2, 3])
        self.assertFalse(replies[1]["result"]["isError"], replies[1])
        self.assertIn("123", replies[1]["result"]["content"][0]["text"])
        events = replies[2]["result"]["content"][0]["text"]
        self.assertTrue(events.startswith("cursor="), events)
        self.assertIn("protocol-noise", events)

    def test_modern_calls_edit_and_compare_only_the_configured_game(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder) / "game"
            shutil.copytree(GAME, game)
            shutil.copyfile(game / "scene.glb", game / "before.glb")
            messages = [modern_call(1, "scene_edit", {
                "action": "set", "object": "Coin1", "assignments": ["position=1,2,3"],
            }), modern_call(2, "scene_get", {"object": "Coin1"}),
                modern_call(3, "scene_diff", {"against": "before.glb"})]
            replies, _ = exchange(game, messages)
            self.assertTrue(all(not reply["result"]["isError"] for reply in replies), replies)
            self.assertIn("position", replies[1]["result"]["content"][0]["text"])
            self.assertIn("+0 -0 ~1 objects", replies[2]["result"]["content"][0]["text"])

    def test_shot_without_a_display_is_a_tool_error_and_the_server_keeps_answering(self):
        env = {key: value for key, value in os.environ.items() if key not in ("DISPLAY", "WAYLAND_DISPLAY")}
        replies, _ = exchange(GAME, [modern_call(1, "game_shot", {"width": 128}),
                                     modern_call(2, "api", {"class": "Transform"})], env=env)
        self.assertTrue(replies[0]["result"]["isError"], replies[0])
        self.assertIn("no display", replies[0]["result"]["content"][0]["text"])
        self.assertFalse(replies[1]["result"]["isError"])

    @unittest.skipUnless(os.environ.get("DISPLAY"), "needs a display")
    def test_shot_returns_a_small_png_image(self):
        replies, _ = exchange(GAME, [modern_call(1, "game_shot", {"width": 128, "at": 0.5})])
        content = replies[0]["result"]["content"]
        self.assertFalse(replies[0]["result"]["isError"], replies[0])
        self.assertEqual(content[0]["mimeType"], "image/png")
        self.assertTrue(base64.b64decode(content[0]["data"]).startswith(PNG_SIGNATURE))
        self.assertIn("128x72", content[1]["text"])

    def test_bad_arguments_are_tool_errors_the_model_can_read(self):
        replies, _ = exchange(GAME, [modern_call(1, "game_run", {"frames": 601})])
        self.assertTrue(replies[0]["result"]["isError"])
        self.assertEqual(replies[0]["result"]["content"][0]["text"], "frames: must be at most 600, not 601")

    def test_read_only_rejects_execution_even_when_it_is_on_the_allow_list(self):
        replies, _ = exchange(GAME, [modern_call(1, "game_run", {"frames": 1})],
                              "--read-only", "--allow", "game_run,api")
        self.assertEqual(replies[0]["error"]["code"], -32602)


if __name__ == "__main__":
    if not PROGRAM.is_file():
        print("skipped: editor executable is not built")
        sys.exit(77)
    unittest.main()
