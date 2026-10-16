"""Says whether a dedicated server's save (its SQLite database) holds one coin for a player: the
platformer's server keeps each player's coins under "players", by the player's identity.

    python3 -I kept_coin.py <save.sqlite3> <identity>
"""
import json
import sqlite3
import sys

PLAYERS_KEY = "players"
KEPT_COINS = 1


def main(save, identity):
    with sqlite3.connect(f"file:{save}?mode=ro", uri=True) as database:
        row = database.execute("SELECT value FROM saves WHERE key = ?", (PLAYERS_KEY,)).fetchone()
    players = json.loads(row[0]) if row else {}
    print("the server's save holds for its players:", players)
    return 0 if players.get(identity, {}).get("coins") == KEPT_COINS else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
