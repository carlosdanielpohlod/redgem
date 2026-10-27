"""Exercise the real stdio server, including native stdout isolation and saved scene edits."""

import json
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


def exchange(game, messages, *flags):
    result = subprocess.run([str(PROGRAM), "mcp", str(game), *flags],
                            input="".join(json.dumps(message) + "\n" for message in messages),
                            text=True, capture_output=True, timeout=60)
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
        self.assertIn("cursor", replies[2]["result"]["structuredContent"])
        self.assertIn("protocol-noise", json.dumps(replies[2]["result"]["structuredContent"]))

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

    def test_read_only_rejects_execution_even_when_it_is_on_the_allow_list(self):
        replies, _ = exchange(GAME, [modern_call(1, "game_run", {"frames": 1})],
                              "--read-only", "--allow", "game_run,api")
        self.assertEqual(replies[0]["error"]["code"], -32602)


if __name__ == "__main__":
    if not PROGRAM.is_file():
        print("skipped: editor executable is not built")
        sys.exit(77)
    unittest.main()
