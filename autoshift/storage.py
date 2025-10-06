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
import operator
from functools import reduce
from typing import TYPE_CHECKING, Any, cast

import peewee as pw

from autoshift.common import _L, Game, Platform, settings

if TYPE_CHECKING:
    from autoshift.models import Key

database = pw.SqliteDatabase(settings.DB_FILE)


def col(prop: Any) -> pw.Field:
    return prop


def get_keys(game_platform_map: dict[Game, set[Platform]]) -> list["Key"]:
    """Get all keys for the given game/platform map"""
    from autoshift.models import Key

    predicate = reduce(
        operator.or_,
        [
            (Key.game == game) & (col(Key.platform).in_(platforms))
            for game, platforms in game_platform_map.items()
        ],
    )

    query = cast(pw.Select, Key.select()).where(
        (Key.redeemed == False)  # noqa: E712
        & (Key.expired == False)  # noqa: E712
        & predicate
    )

    keys = list(query)
    _L.debug(f"Found {len(keys)} redeemable keys")
    return keys
