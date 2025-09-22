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
import logging
import os
from typing import Annotated

import typer
from typer import Typer

from src.common import _L, Game, Platform, settings
from src.models import Key
from src.query import known_games
from src.shift import ShiftClient, Status

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


def redeem(key: Key):
    """Redeem key and set as redeemed if successfull"""

    _L.info(f"Trying to redeem {key.reward} ({key.code})")
    status = client.redeem(key, known_games.get(Game[key.game]))
    _L.debug(f"Status: {status}")

    # notify user
    try:
        # this may fail if there are other `{<something>}` in the string..
        _L.info("  " + status.msg.format(**locals()))
    except Exception:
        _L.info("  " + status.msg)

    return status


def query_keys(games: list[Game], platforms: list[Platform]):
    """Query new keys for given games and platforms

    Returns dict of dicts of lists with [game][platform] as keys"""
    from src import query

    # TODO
    num_keys_b4 = 0  # query.db.num_keys
    # parse all keys
    query.fetch_keys()
    num_keys_after = 0  # query.db.num_keys

    new_keys = list(query.db.get_keys(games, platforms))

    diff = num_keys_after - num_keys_b4
    _L.info(f"done. ({diff or 'no'} new Keys)")

    return new_keys


@app.callback()
def callback(
    games: Annotated[
        list[Game] | None,
        typer.Option(
            "--game", envvar="SHIFT_GAMES", help="Games you want to query SHiFT keys for"
        ),
    ] = None,
    platforms: Annotated[
        list[Platform] | None,
        typer.Option(
            "--platforms",
            envvar="SHIFT_PLATFORMS",
            help="Platforms you want to query SHiFT keys for",
        ),
    ] = None,
    user: Annotated[
        str | None,
        typer.Option(
            "--user",
            "-u",
            envvar="SHIFT_USER",
            help="User login (optional, will prompt if not provided)",
        ),
    ] = None,
    password: Annotated[
        str | None,
        typer.Option(
            "--pass",
            "-p",
            envvar="SHIFT_PASS",
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


@app.command("redeem")
def redeem_one(code: Annotated[str, typer.Argument()]):
    if not settings.PLATFORMS:
        settings.PLATFORMS = [typer.prompt("Select a platform", type=Platform)]
    db_key = Key.get((Key.code == code) & (Key.platform == settings.PLATFORMS[0]))

    if not db_key:
        db_key = Key.create(
            code=code,
            platform=settings.PLATFORMS[0],
            game="UNKNOWN",
        )
    redeem(db_key)


@app.command("schedule")
def auto_redeem_codes(
    schedule: Annotated[
        int,
        typer.Argument(
            envvar="SHIFT_SCHEDULE",
            min=120,
            help="Keep checking for keys every N minutes (minimum 120)",
        ),
    ] = 120,
    limit: Annotated[
        int | None,
        typer.Option(
            "--limit",
            "-l",
            envvar="SHIFT_LIMIT",
            min=1,
            help="Number of golden keys to redeem",
        ),
    ] = None,
):
    """Redeem SHiFT codes for specified games and platforms."""

    if schedule:
        settings.SCHEDULE = schedule
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
    all_keys = query_keys(settings.GAMES, settings.PLATFORMS)

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
