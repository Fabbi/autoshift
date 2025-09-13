#############################################################################
#
# Copyright (C) 2018 Fabian Schweinfurth
# Contact: autoshift <at> derfabbi.de
#
# This file is part of autoshift
#
# autoshift is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# autoshift is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with autoshift.  If not, see <http://www.gnu.org/licenses/>.
#
#############################################################################
import re
import sqlite3
from os import makedirs, path
from typing import (Callable, ContextManager, Dict, Generic, Iterable,
                    Iterator, Optional, TypeVar)

import requests

from common import _L, DIRNAME

_KT = TypeVar("_KT")
_VT = TypeVar("_VT")

class SymmetricDict(Dict[_KT, _VT], Generic[_KT, _VT]):
    class ValueOverlapError(Exception):
        pass

    inv: Dict[_VT, _KT]

    def __init__(self, *args, **kwargs):
        self.inv = {}
        self.update(*args, **kwargs)

    def __setitem__(self, k: _KT, v: _VT) -> None:
        ret = dict.__setitem__(self, k, v)

        if v in self.inv and self.inv[v] != k:
            raise SymmetricDict.ValueOverlapError(f"Key `{v}` already exists in inverted dict!")

        self.inv[v] = k
        return ret

    def update(self, *args, **kwargs):
        for k, v in dict(*args, **kwargs).items():
            self[k] = v

    def without(self, *args):
        ret = SymmetricDict(self)
        for arg in args:
            del ret[arg]
        return ret

### games used to help find the correct shift redemption forms
known_games = SymmetricDict({
    "bl1":  "Borderlands: Game of the Year Edition",
    "bl2":  "Borderlands 2",
    "bl3":  "Borderlands 3",
    "bl4":  "Borderlands 4",
    "blps": "Borderlands: The Pre-Sequel",
    "ttw":  "Tiny Tina's Wonderland",
    "gdfll": "Godfall",
})

### platforms that are used to find the correct input values in the shift redemption forms
known_platforms = SymmetricDict({
    "steam":     "steam",
    "epic":      "epic",
    "psn":       "playstation",
    "xboxlive":  "xbox",
    "nintendo":  "nintendo",
    "stadia":    "", # this one could be a substring
    "universal": "universal"
})



spaces = re.compile(r"\s")
r_word_chars = re.compile(r"(the|[^a-z0-9])", re.IGNORECASE)
lowercase_chars = re.compile(r"[a-z]")
vowels = re.compile(r"[aeiou]", re.IGNORECASE)
r_golden_keys = re.compile(r"^(\d+)?.*(gold|skelet).*", re.IGNORECASE)

def print_banner(data):
    lines = []
    try:
        lines.extend(data["meta"][attr] for attr in ("attribution", "permalink"))
    except Exception:
        lines.append("Codes provided by Orcicorn")
        lines.append("@ https://shift.orcicorn.com/shift-code/")

    longest_line = max(len(line) for line in lines) + 2
    banner = "\n".join(f"{line: ^{longest_line}}"
                       for line in lines)
    txt = " autoshift by @Fabbi "
    banner = f"{txt:=^{longest_line}}\n{banner}\n"
    banner += "="*longest_line
    _L.info(f"\r\033[1;5;31m{banner}\n")


def get_short_game_key(game: str) -> str:
    if game in known_games.inv:
        return known_games.inv[game]

    ret = game
    if game.lower() == "wonderlands":
        ret = "ttw"
    elif not any(spaces.finditer(game)):
        ret = vowels.sub("", game).lower()
    else:
        ret = ret.replace("Borderlands", "BL")
        ret = r_word_chars.sub("", ret)
        ret = lowercase_chars.sub("", ret).lower()

    if ret not in known_games:
        known_games[ret] = game
        _L.info(f"Found new game: {game}")
        db.saw_game(ret, game)
    return ret

def get_short_platform_key(platform: str) -> str:
    if platform.lower() in known_platforms.inv:
        return known_platforms.inv[platform.lower()]

    # check if a known platform could match this one..
    for shift_platform in known_platforms.keys():
        if shift_platform in platform.lower():
            _L.info(f"Handling platform `{platform}` as `{shift_platform}`")
            platform = known_platforms[shift_platform]
            if not platform:
                # this one we don't know the "long" version of, yet.
                platform = platform.lower()
                known_platforms[shift_platform] = platform
                db.saw_platform(shift_platform, platform)
            break

    if not platform:
        # didn't find a possible replacement
        platform = platform.lower()
        _L.error(f"Didn't understand platform `{platform}`. "
                 "Please contact the developer @ github.com/Fabbi")

    return platform


class Key:
    __slots__ = ("id", "reward", "code", "game", "platform", "redeemed",
                 "type", "archived", "expires", "link", "expired")
    def __init__(self, **kwargs):
        self.redeemed = False
        self.id = None
        self.set(**kwargs)

    def set(self, **kwargs):
        for k in kwargs:
            setattr(self, k, kwargs[k])
        return self

    def copy(self):
        return Key(**{k: getattr(self, k)
                      for k in self.__slots__
                      if hasattr(self, k)})

    def __str__(self): # noqa
        return "Key({})".format(
            ", ".join([str(getattr(self, k))
                       for k in ("id", "reward", "code", "game", "platform", "redeemed")]))
    def __repr__(self): # noqa
        return str(self)

class Database(ContextManager):
    __conn: sqlite3.Connection
    __c: sqlite3.Cursor
    version: int

    def __init__(self):
        self.__updated = False
        self.__open = False
        self.__create_db = not path.exists(path.join(DIRNAME, "data", "keys.db"))
        self.__open_db()
        self.version = self.__c.execute("PRAGMA user_version").fetchone()[0]

        if self.version >= 1:
            for _k in ("game", "platform"):
                ex = self.__c.execute(f"SELECT * from seen_{_k}s;").fetchall()
                for row in ex:
                    globals()[f"known_{_k}s"][row["key"]] = row["name"]

    def __enter__(self):
        self.__open_db()
        return self

    def __exit__(self, *_) -> Optional[bool]:
        self.close_db()
        return False

    def execute(self, sql, parameters=None):
        if not self.__updated:
            self.__update_db()
        if parameters is not None:
            return self.__c.execute(sql, parameters)
        return self.__c.execute(sql)

    def commit(self):
        self.__conn.commit()

    def __update_db(self):
        import sys

        from migrations import migrationFunctions

        # self.close_db()
        # self.__open_db()
        while (self.version + 1) in migrationFunctions:
            if not self.__create_db:
                _L.info(f"Migrating database to version {self.version+1}")
            func = migrationFunctions[self.version + 1]

            if not func(self.__conn, self.__create_db):
                sys.exit(1)
            if not self.__create_db:
                _L.info(f"migration to version {self.version+1} successful")
            self.version += 1

        self.__updated = True

    def __open_db(self):
        if self.__open:
            return

        makedirs(path.join(DIRNAME, "data"), exist_ok=True)
        self.__conn = sqlite3.connect(path.join(DIRNAME, "data", "keys.db"),
                            detect_types=sqlite3.PARSE_DECLTYPES)
        self.__conn.row_factory = sqlite3.Row
        self.__c = self.__conn.cursor()

        self.__c.execute("CREATE TABLE IF NOT EXISTS keys "
                "(id INTEGER primary key, description TEXT, "
                "key TEXT, platform TEXT, game TEXT, redeemed INTEGER)")
        self.commit()
        self.__open = True


    def close_db(self):
        if self.__open:
            self.__conn.commit()
            self.__conn.close()
            self.__open = False


    def insert(self, key: Key):
        """Insert key"""

        el = self.execute("""SELECT * FROM keys
                    WHERE platform = ?
                    AND code = ?
                    AND game = ?""",
                    (key.platform, key.code, key.game))
        if el.fetchone():
            return None
        _L.debug(f"== inserting {key.game} Key '{key.code}' for {key.platform} ==")
        self.execute("INSERT INTO keys(reward, code, platform, game, redeemed) "
                     "VAlUES (?,?,?,?,0)",
                     (key.reward, key.code, key.platform, key.game))
        self.commit()
        return key


    def get_keys(self, platform, game, all_keys=False):
        """Get all (unredeemed) keys of given platform and game"""
        cmd = """
            SELECT * FROM keys"""
        params = []

        if platform:
            cmd += " WHERE (platform=? OR platform='universal')"
            params.append(platform)

        if game:
            params.append(game)
            kw = " AND" if platform else " WHERE"
            cmd += f"{kw} game=?"
        if not all_keys:
            kw = " AND" if (platform or game) else " WHERE"
            cmd += f"{kw} redeemed=0"

        cmd += " ORDER BY id DESC"
        ex = self.execute(cmd, params)

        # keys = []
        row: sqlite3.Row
        for row in ex.fetchall():
            yield Key(**{k:row[k] for k in row.keys()})
            # keys.append(Key(*row))

        # return keys


    def get_special_keys(self, platform, game):
        keys = self.get_keys(platform, game)
        num = 0
        ret = []
        for k in keys:
            if not r_golden_keys.match(k.reward):
                num += 1
                ret.append(k)
        return num, ret


    def get_golden_keys(self, platform, game, all_keys=False):
        keys = self.get_keys(platform, game, all_keys)
        num = 0
        ret = []
        for k in keys:
            m = r_golden_keys.match(k.reward)
            if m is not None:
                num += int(m.group(1) or 1)
                ret.append(k)
        return num, ret


    def set_redeemed(self, key):
        key.redeemed = 1
        self.execute("UPDATE keys SET redeemed=1 WHERE id=(?)", (key.id, ))
        self.commit()

    def saw_game(self, short, name):
        self.execute("INSERT into seen_games(key, name) VALUES (?, ?)", (short, name))
        self.commit()
    def saw_platform(self, short, name):
        self.execute("INSERT into seen_platforms(key, name) VALUES (?, ?)", (short, name))
        self.commit()



special_key_handler: dict[str, Callable[[Key], list[Key]]] = {
    "Borderlands 2 and 3": lambda key: [key.copy().set(game="Borderlands 2"),
                                   key.set(game="Borderlands 3")],
    "Borderlands": lambda key: [key.set(game="Borderlands 1")]
    # "Universal": lambda key: (key.copy().set(platform=plat) for plat in platforms)
}

def flatten(itr: Iterable[Iterable[_VT]]) -> Iterator[_VT]:
    for el in itr:
        yield from el


def progn(*args: _VT) -> _VT:
    *_, lastArg = args
    return lastArg

def parse_shift_orcicorn():
    import json
    key_url = "https://raw.githubusercontent.com/ugoogalizer/autoshift-codes/main/shiftcodes.json"


    resp = requests.get(key_url)
    if not resp:
        _L.error(f"Error querying for new keys: {resp.reason}")
        return None

    data: dict = json.loads(resp.text)[0]

    if "codes" not in data:
        _L.error("Invalid response. Please contact the developer @ github.com/fabbi")
        return None

    # Remove expired keys by default (by creating a new dict without them)
    valid_codes = []
    for code_data in data["codes"]:
        if code_data['expired'] == True:
            continue
        else:
            valid_codes.append(code_data)

    if parse_shift_orcicorn.first_parse:
        parse_shift_orcicorn.first_parse = False
        print_banner(data)

    for code_data in valid_codes:
        keys: Iterable[Key]= [Key(**code_data)]

        # 1. special_key_handler
        # 2. known platform
        # 3. shorten game
        keys = list(
            flatten(map(lambda key: special_key_handler[key.game](key)
                               if key.game in special_key_handler
                               else [key]
                        , keys)))

        for key in keys:
            key.set(game=get_short_game_key(key.game))
            key.set(platform=get_short_platform_key(key.platform))

        yield from keys

parse_shift_orcicorn.first_parse = True


def update_keys():
    from collections import Counter
    if not parse_shift_orcicorn.first_parse:
        _L.info("Checking for new keys!")

    keys = list(parse_shift_orcicorn())
    new_keys = [db.insert(key) for key in keys]

    counts = Counter(key.game for key in new_keys if key)
    for game, count in sorted(counts.items()):
        _L.info(f"Got {count} new keys for {known_games[game]}")

    return keys


db = Database()
