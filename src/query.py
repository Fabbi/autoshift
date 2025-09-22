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
from collections.abc import Callable, Iterable, Iterator
from typing import (
    Any,
    TypeVar,
)

from peewee import DoesNotExist

from src.common import _L, Game, Platform, settings
from src.models import Key

_KT = TypeVar("_KT")
_VT = TypeVar("_VT")


class SymmetricDict[KT, VT](dict[KT, VT]):
    class ValueOverlapError(Exception):
        pass

    inv: dict[VT, KT]

    def __init__(self, *args, **kwargs):
        self.inv = {}
        self.update(*args, **kwargs)

    def __setitem__(self, k: KT, v: VT) -> None:
        ret = dict.__setitem__(self, k, v)

        if v in self.inv and self.inv[v] != k:
            raise SymmetricDict.ValueOverlapError(
                f"Key `{v}` already exists in inverted dict!"
            )

        self.inv[v] = k
        return ret

    def update(self, *args, **kwargs):
        for k, v in dict(*args, **kwargs).items():
            self[k] = v  # pyright: ignore[reportArgumentType]

    def without(self, *args):
        ret = SymmetricDict(self)
        for arg in args:
            del ret[arg]
        return ret


### games used to help find the correct shift redemption forms
known_games = SymmetricDict[Game, str](
    {
        Game.bl1: "Borderlands: Game of the Year Edition",
        Game.bl2: "Borderlands 2",
        Game.bl3: "Borderlands 3",
        Game.bl4: "Borderlands 4",
        Game.blps: "Borderlands: The Pre-Sequel",
        Game.ttw: "Tiny Tina's Wonderland",
        Game.gdfll: "Godfall",
    }
)

### platforms that are used to find the correct input values in the shift redemption forms
known_platforms = SymmetricDict(
    {
        Platform.steam: "steam",
        Platform.epic: "epic",
        Platform.psn: "playstation",
        Platform.xboxlive: "xbox",
        Platform.stadia: "",  # this one could be a substring
        "universal": "universal",
    }
)


spaces = re.compile(r"\s")
r_word_chars = re.compile(r"(the|[^a-z0-9])", re.IGNORECASE)
lowercase_chars = re.compile(r"[a-z]")
vowels = re.compile(r"[aeiou]", re.IGNORECASE)
r_golden_keys = re.compile(r"^(\d+)?.*(gold|skelet).*", re.IGNORECASE)


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
            break

    if not platform:
        # didn't find a possible replacement
        platform = platform.lower()
        _L.error(
            f"Didn't understand platform `{platform}`. "
            "Please contact the developer @ github.com/Fabbi"
        )

    return platform


class DatabaseManager:
    """Simple database manager for key operations."""

    def insert(self, key_data: dict) -> Key | None:
        """Insert a key if it doesn't already exist."""
        try:
            # Check if key already exists
            Key.get(
                (Key.code == key_data["code"])
                & (Key.platform == key_data["platform"])
                & (Key.game == key_data["game"])
            )
            return None  # Key already exists
        except DoesNotExist:
            # Create new key
            key = Key.create(
                **key_data,
                redeemed=False,
            )
            _L.debug(f"== inserting {key.game} Key '{key.code}' for {key.platform} ==")
            return key

    def get_keys(self, games: list[Game], platforms: list[Platform]) -> Iterator[Key]:
        """Get keys matching platform and game criteria."""
        query = (
            Key.select()
            .where(
                (Key.platform.in_(platforms)) | (Key.platform == "universal")  # pyright: ignore[reportCallIssue]
            )
            .where(Key.game.in_(games))
        )

        query = query.order_by(Key.id.desc())

        yield from query

    def set_redeemed(self, key: Key) -> None:
        """Mark a key as redeemed."""
        key.redeemed = True
        key.save()


special_key_handler: dict[str, Callable] = {
    "Borderlands 2 and 3": lambda key: [
        key.copy().set(game="Borderlands 2"),
        key.set(game="Borderlands 3"),
    ],
    "Borderlands": lambda key: [key.set(game="Borderlands 1")],
    # "Universal": lambda key: (key.copy().set(platform=plat) for plat in platforms)
}


def flatten[T](itr: Iterable[Iterable[T]]) -> Iterator[T]:
    for el in itr:
        yield from el


def progn[T](*args: T) -> T:
    *_, lastArg = args
    return lastArg


Fetcher = Any
key_fetcher: dict[Game, list[Fetcher]] = {}


def fetch_keys():
    all_new_keys: list[Key] = []
    for game in settings.GAMES:
        for fetcher in key_fetcher[game]:
            all_new_keys.extend(fetcher())
    Key.bulk_create(all_new_keys)

    return all_new_keys


db = DatabaseManager()

"""
https://www.ign.com/wikis/borderlands-4/Borderlands_4_SHiFT_Codes
https://www.ign.com/wikis/borderlands-3/Borderlands_3_SHiFT_Codes

"""
