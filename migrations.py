import sqlite3
from typing import List, Union

from common import _L

migrationFunctions = {}

def register(version: int):
    from functools import wraps
    def wrap(func):
        @wraps(func)
        def wrapper(conn):
            try:
                if func(conn):
                    conn.cursor().execute(f"PRAGMA user_version = {version}")
                    conn.commit()
                    return True
            except sqlite3.OperationalError:
                _L.error(f"There was an error while migrating database to version {version}."
                        "Please contact the developer @ github.com/fabbi")
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
        """
        ,
        """
        ALTER TABLE keys
        RENAME COLUMN key TO code;
        """
        ,
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
        """
        ,
        """
        UPDATE keys
        SET game="bl1"
        WHERE game="bl";
        """
        ,
        """
        CREATE TABLE IF NOT EXISTS seen_platforms
        (key TEXT primary key, name TEXT)
        """
        ,
        """
        CREATE TABLE IF NOT EXISTS seen_games
        (key TEXT primary key, name TEXT)
        """
    ]

    from query import known_games, known_platforms
    for game in known_games.items():
        steps.append(("""
        INSERT INTO seen_games(key, name)
        VALUES(?, ?)
        """, game))
    for platform in known_platforms.items():
        steps.append(("""
        INSERT INTO seen_platforms(key, name)
        VALUES(?, ?)
        """, platform))

    for i, step in enumerate(steps):
        _L.debug(f"executing step {i} of update_1")
        try:
            if isinstance(step, tuple):
                c.execute(*step)
            else:
                c.executescript(step)
        except sqlite3.OperationalError as e:
            _L.error(f"'{e}' in\n{step}")
            return False
    return True