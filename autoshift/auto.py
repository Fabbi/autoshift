#!/usr/bin/env python
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
from enum import Enum
import json
import logging
import os
import re
from collections.abc import Sequence
from typing import (
    TYPE_CHECKING,
    Annotated,
    Literal,
    cast,
)

import httpx
import typer
from typer import Typer

from autoshift import storage
from autoshift.common import _L, Game, Platform, settings
from autoshift.migrations import run_migrations
from autoshift.models import Key
from autoshift.shift import ShiftClient, Status

LICENSE_TEXT = """\
========================================================================
autoshift  Copyright (C) 2019  Fabian Schweinfurth
This program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.
This is free software, and you are welcome to redistribute it
under certain conditions; see LICENSE for details.
========================================================================
"""


app = Typer(help="AutoSHiFt: Automatically redeem Gearbox SHiFT Codes")

client: ShiftClient = ShiftClient()

r_golden_keys = re.compile(r"(\d+) (?:gold|skelet).*key", re.IGNORECASE)


def redeem(key: Key):
    """Redeem key and set as redeemed if successfull"""

    _L.info(f"Trying to redeem {key.reward} ({key.code})")
    status = client.redeem(key)
    _L.debug(f"Status: {status}")

    # notify user
    try:
        # this may fail if there are other `{<something>}` in the string..
        _L.info("  " + status.msg.format(**locals()))
    except Exception:
        _L.info("  " + status.msg)

    return status


def clean_key_data(key_data: Sequence[dict]):
    for key in key_data:
        data = {
            k: v
            for k in Key._meta.sorted_field_names
            # for now we remove expiration dates
            # as the format seems to be very inconsistent
            if k != "expires" and (v := key.get(k))
        }

        golden_match = r_golden_keys.search(data["reward"])
        if golden_match:
            data["num_golden"] = int(golden_match.group(1))
            if data["num_golden"] > 2000:
                # there are keys like `SDCC 2024 Golden Keys`
                # which obviously don't give 2024 keys
                data["num_golden"] = 1

        data["game"] = Game(data["game"])
        if data["platform"] == "universal":
            yield from ({**data, "platform": platform} for platform in Platform)
        else:
            data["platform"] = Platform(data["platform"])
            yield data


def query_keys(game_map: dict[Game, list[Platform]]) -> list[Key]:
    """Query new keys for given games and platforms

    Returns dict of dicts of lists with [game][platform] as keys"""

    # parse all keys
    if settings.SHIFT_SOURCE:
        if settings.SHIFT_SOURCE.startswith("http"):
            key_data = httpx.get(settings.SHIFT_SOURCE).json()
        else:
            with open(settings.SHIFT_SOURCE) as f:
                key_data = json.load(f)

        keys = clean_key_data(key_data[0]["codes"])

        num_new_keys = Key.insert_many(keys).on_conflict_ignore().execute()
        _L.info(f"{num_new_keys or 'no'} new Keys")

    new_keys = storage.get_keys(game_map)

    return new_keys


@app.callback()
def callback(
    games: Annotated[
        list[Game] | None,
        typer.Option("--game", help="Games you want to query SHiFT keys for"),
    ] = None,
    platforms: Annotated[
        list[Platform] | None,
        typer.Option(
            "--platforms",
            help="Platforms you want to query SHiFT keys for",
        ),
    ] = None,
    user: Annotated[
        str | None,
        typer.Option(
            "--user",
            "-u",
            help="User login (optional, will prompt if not provided)",
        ),
    ] = None,
    password: Annotated[
        str | None,
        typer.Option(
            "--pass",
            "-p",
            help="Password for your login (optional, will prompt if not provided)",
        ),
    ] = None,
    verbose: Annotated[
        bool, typer.Option("--verbose", "-v", help="Enable verbose mode")
    ] = False,
):
    if not user:
        user = settings.USER

    if not password and settings.PASS:
        password = settings.PASS.get_secret_value()

    if games:
        settings.GAMES = games
    if platforms:
        settings.PLATFORMS = platforms

    # Set up logging
    if verbose:
        _L.setLevel(logging.DEBUG)
        for handler in _L.handlers:
            handler.setLevel(logging.DEBUG)
        _L.debug("Debug mode on")

    # Check for first-time usage
    if not settings.COOKIE_FILE.exists():
        typer.echo(LICENSE_TEXT)

    # try logging in first. Does nothing if already logged in
    client.login(user, password)

    from autoshift.storage import database

    database.connect()
    run_migrations(database)


@app.command(
    "redeem", context_settings=dict(allow_extra_args=True, ignore_unknown_options=True)
)
def redeem_one(
    platform: Annotated[Platform, typer.Argument()],
    code: Annotated[str, typer.Argument()],
):
    db_key = Key.get_or_none((Key.code == code) & (Key.platform == platform))

    if not db_key:
        db_key = Key.create(
            code=code,
            platform=platform,
            game="UNKNOWN",
        )
    redeem(db_key)


# PlatformArg = Literal[[p.name for p in Platform] + ["all"]]
PlatformArg = Enum("Platform", [(p.name, p.value) for p in Platform] + [("all", "all")])


@app.command(
    "schedule",
)
def auto_redeem_codes(
    bl1: Annotated[list[PlatformArg] | None, typer.Option()] = None,
    bl2: Annotated[list[PlatformArg] | None, typer.Option()] = None,
    bl3: Annotated[list[PlatformArg] | None, typer.Option()] = None,
    bl4: Annotated[list[PlatformArg] | None, typer.Option()] = None,
    blps: Annotated[list[PlatformArg] | None, typer.Option()] = None,
    ttw: Annotated[list[PlatformArg] | None, typer.Option()] = None,
    gdfll: Annotated[list[PlatformArg] | None, typer.Option()] = None,
    interval: Annotated[
        int,
        typer.Option(
            help="Keep checking for keys every N minutes (minimum 120)",
        ),
    ] = 120,
    limit: Annotated[
        int | None,
        typer.Option(
            "--limit",
            "-l",
            min=1,
            help="Number of golden keys to redeem",
        ),
    ] = None,
):
    """Redeem SHiFT codes for specified games and platforms."""
    if TYPE_CHECKING:
        _silence_unused = (bl1, bl2, bl3, bl4, blps, ttw, gdfll)

    game_platform_map: dict[Game, list[Platform]] = {}
    for game in Game:
        value = locals().get(game.name)
        if value is None:
            continue
        if any(v == PlatformArg.all for v in value):  # pyright: ignore[reportAttributeAccessIssue]
            value = list(Platform)
        game_platform_map[game] = value

    if game_platform_map:
        settings._GAMES_PLATFORM_MAP.update(game_platform_map)

    settings.SCHEDULE = interval

    if limit:
        settings.LIMIT = limit

    # Execute main logic
    main()

    # Handle scheduling
    if settings.SCHEDULE:
        hours, minutes = divmod(settings.SCHEDULE, 60)
        _L.info(f"Scheduling to run every {hours:02}:{minutes:02} hours")
        from apscheduler.schedulers.blocking import BlockingScheduler

        scheduler = BlockingScheduler()
        scheduler.add_job(
            main,
            "interval",
            minutes=settings.SCHEDULE,
        )
        typer.echo(f"Press Ctrl+{'Break' if os.name == 'nt' else 'C'} to exit")

        try:
            scheduler.start()
        except (KeyboardInterrupt, SystemExit):
            pass

    _L.info("Goodbye.")


def main():
    from time import sleep

    # query all keys
    all_keys = query_keys(settings._GAMES_PLATFORM_MAP)

    _L.info("Trying to redeem now.")

    for num, key in enumerate(all_keys):
        if num and not (num % 15):
            _L.info("Trying to prevent a 'too many requests'-block.")
            sleep(60)

        status = redeem(key)
        # don't spam if we reached the hourly limit
        if status == Status.TRYLATER:
            return

    _L.info("No more keys left!")


if __name__ == "__main__":
    app()
