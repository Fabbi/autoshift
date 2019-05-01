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
import sqlite3
import requests
from bs4 import BeautifulSoup as BSoup

from common import _L, DIRNAME

platforms = ["pc", "ps", "xbox"]
# will be filled later
games = []
game_funcs = {}


conn = None
c = None


def registerParser(*the_games):
    """Register parser for specified games.

    e.g:

        @registerParser("bl3")
        def parse_bl3(game, platform):
            description = "some key"
            code = "AAAA-BBBBB-CCCCC-DDDDD"
            yield description, code, platform, game
    """
    def decorator(f):
        global game_funcs, games
        for game in the_games:
            game_funcs[game] = f
            games.append(game)
        return f
    return decorator


class Key:
    """small class like `functools.namedtuple` but with settable attributes"""
    __slots__ = ("id", "description", "key", "redeemed")
    def __init__(self, *args, **kwargs): # noqa
        for el in Key.__slots__:
            setattr(self, el, None)
        for i, el in enumerate(args):
            setattr(self, Key.__slots__[i], el)
        for k in kwargs:
            setattr(self, k, kwargs[k])
    def __str__(self): # noqa
        return "Key({})".format(
            ", ".join([str(getattr(self, k))
                       for k in Key.__slots__]))
    def __repr__(self): # noqa
        return str(self)


def open_db():
    from os import path
    global conn, c
    conn = sqlite3.connect(path.join(DIRNAME, "keys.db"),
                           detect_types=sqlite3.PARSE_DECLTYPES)
    c = conn.cursor()

    c.execute("CREATE TABLE IF NOT EXISTS keys "
              "(id INTEGER primary key, description TEXT, "
              "key TEXT, platform TEXT, game TEXT, redeemed INTEGER)")


def close_db():
    conn.commit()
    conn.close()


def insert(desc, code, platform, game):
    """Insert Key"""
    el = c.execute("""SELECT * FROM keys
                   WHERE platform = ?
                   AND key = ?
                   AND game = ?""",
                   (platform, code, game))
    if el.fetchone():
        return
    _L.debug("== inserting {} Key '{}' for {} ==".format(game.upper(), code,
                                                         platform.upper()))
    c.execute("INSERT INTO keys(description, key, platform, game, redeemed) "
              "VAlUES (?,?,?,?,0)", (desc, code, platform, game))
    conn.commit()


def get_keys(platform, game, all_keys=False):
    """Get all (unredeemed) keys of given platform and game"""
    cmd = """
        SELECT id, description, key, redeemed FROM keys
        WHERE platform=?
        AND game=?"""
    if not all_keys:
        cmd += " AND redeemed=0"
    cmd += " ORDER BY id DESC"
    ex = c.execute(cmd, (platform, game))

    keys = []
    for row in ex:
        keys.append(Key(*row))

    return keys


def get_special_keys(platform, game):
    keys = get_keys(platform, game)
    num = 0
    ret = []
    for k in keys:
        if "gold" not in k.description.lower():
            num += 1
            ret.append(k)
    return num, ret


def get_golden_keys(platform, game, all_keys=False):
    import re
    # quite general regex..
    # you never know what they write in those tables..
    g_reg = re.compile(r"^(\d+).*gold.*", re.I)
    keys = get_keys(platform, game, all_keys)
    num = 0
    ret = []
    for k in keys:
        m = g_reg.match(k.description)
        if m:
            num += int(m.group(1))
            ret.append(k)
    return num, ret


def set_redeemed(key):
    key.redeemed = 1
    c.execute("UPDATE keys SET redeemed=1 WHERE id=(?)", (key.id, ))
    conn.commit()


def parse_keys(game, platform):
    if game not in games:
        _L.error("No known method of retrieving new SHiFT codes "
                 "for the game `{}`".format(game))
        return

    for code in game_funcs[game](game, platform):
        insert(*code)


@registerParser("bl", "bl2", "blps")
def parse_bl2blps(game, platform):
    """Get all Keys from orcz"""
    key_urls = {"bl": "http://orcz.com/Borderlands:_Golden_Key",
                "bl2": "http://orcz.com/Borderlands_2:_Golden_Key",
                "blps": "http://orcz.com/Borderlands_Pre-Sequel:_Shift_Codes",
                # "bl3": "http://orcz.com/Borderlands_3:_Golden_Key"
                }

    def check(key):
        """Check if key is expired (return None if so)"""
        ret = None
        span = key.find("span")
        if (not span) or (not span.get("style")) or ("black" in span["style"]):
            ret = key.text.strip()

            # must be of format ABCDE-FGHIJ-KLMNO-PQRST
            if ret.count("-") != 4:
                return None
            spl = ret.split("-")
            spl = [len(el) == 5 for el in spl]
            if not all(spl):
                return None

        return ret

    r = requests.get(key_urls[game])
    soup = BSoup(r.text, "lxml")
    table = soup.find("table")
    rows = table.find_all("tr")[1:]
    for row in rows:
        cols = row.find_all("td")
        desc = cols[1].text.strip()
        codes = [None, None, None]
        for i in range(4, 7):
            try:
                codes[i - 4] = check(cols[i])
            except Exception as e: # noqa
                _L.debug(e)
                pass

        for i in range(3):
            if codes[i]:
                yield desc, codes[i], platforms[i], game
