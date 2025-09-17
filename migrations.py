import os
import re
import sqlite3
from typing import List, Union
from common import DIRNAME, _L

DB_PATH = os.path.join(DIRNAME, "data", "keys.db")
CODE_PATTERN = re.compile(r"^[A-Za-z0-9]{5}(?:-[A-Za-z0-9]{5}){4}$")


def migrate_shift_codes():
    """Remove codes from the database that do not match the required format."""
    if not os.path.exists(DB_PATH):
        return
    conn = sqlite3.connect(DB_PATH)
    try:
        c = conn.cursor()
        c.execute("SELECT id, code FROM keys")
        rows = c.fetchall()
        to_remove = [
            (row[0], row[1]) for row in rows if not CODE_PATTERN.match(str(row[1]))
        ]
        if to_remove:
            c.executemany("DELETE FROM keys WHERE id = ?", [(i,) for i, _ in to_remove])
            conn.commit()
            removed_codes = [code for _, code in to_remove]
            _L.info(
                f"Migration: Removed {len(removed_codes)} invalid shift codes from database."
            )
            for code in removed_codes:
                _L.info(f"Removed code: {code}")
    finally:
        conn.close()


# --- Versioned schema migrations ---

migrationFunctions = {}


def register(version: int):
    from functools import wraps

    def wrap(func):
        @wraps(func)
        def wrapper(conn):
            try:
                if func(conn):
                    conn.cursor().execute("PRAGMA user_version = {}".format(version))
                    conn.commit()
                    return True
            except sqlite3.OperationalError:
                _L.error(
                    "There was an error while migrating database to version {}."
                    "Please contact the developer @ github.com/fabbi".format(version)
                )
                return False

        migrationFunctions[version] = wrapper
        return wrapper

    return wrap


@register(1)
def update_1(conn: sqlite3.Connection):
    c = conn.cursor()

    steps: List[Union[str, tuple]] = [
        """
        ALTER TABLE keys
        RENAME COLUMN description TO reward;
        """,
        """
        ALTER TABLE keys
        RENAME COLUMN key TO code;
        """,
        """
        CREATE VIEW tmp AS
        SELECT * FROM keys
        WHERE platform="pc";
        -- AND redeemed=0;

        INSERT INTO keys(reward, code, platform, game, redeemed)
        SELECT reward, code, "epic", game, redeemed
        FROM tmp;

        UPDATE keys
        SET platform="steam"
        WHERE id in (select id from tmp);

        DROP VIEW tmp;
        """,
        """
        UPDATE keys
        SET game="bl1"
        WHERE game="bl";
        """,
        """
        CREATE TABLE IF NOT EXISTS seen_platforms
        (key TEXT primary key, name TEXT)
        """,
        """
        CREATE TABLE IF NOT EXISTS seen_games
        (key TEXT primary key, name TEXT)
        """,
    ]

    from query import known_games, known_platforms

    for game in known_games.items():
        steps.append(("""INSERT INTO seen_games(key, name) VALUES(?, ?)""", game))
    for platform in known_platforms.items():
        steps.append(
            ("""INSERT INTO seen_platforms(key, name) VALUES(?, ?)""", platform)
        )

    for i, step in enumerate(steps):
        _L.debug("executing step {} of update_1".format(i))
        try:
            if isinstance(step, tuple):
                c.execute(*step)
            else:
                c.executescript(step)
        except sqlite3.OperationalError as e:
            _L.error(f"'{e}' in\n{step}")
            return False
    return True
