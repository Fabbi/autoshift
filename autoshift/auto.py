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
import json
import logging
import os
import re
import sys
from collections.abc import Callable, Sequence
from enum import Enum
from typing import (
    TYPE_CHECKING,
    Annotated,
    cast,
)

import click
import httpx
import typer
from pydantic import SecretStr
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


app = Typer(help="AutoSHiFT: Automatically redeem Gearbox SHiFT Codes")

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


def query_keys(game_map: dict[Game, set[Platform]]) -> list[Key]:
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


PlatformArg = Enum("Platform", [(p.name, p.value) for p in Platform] + [("all", "all")])


@app.callback()
def callback(
    ctx: typer.Context,
    bl1: Annotated[
        list[PlatformArg] | None, typer.Option(rich_help_panel="Platform Selection")
    ] = None,
    bl2: Annotated[
        list[PlatformArg] | None, typer.Option(rich_help_panel="Platform Selection")
    ] = None,
    bl3: Annotated[
        list[PlatformArg] | None, typer.Option(rich_help_panel="Platform Selection")
    ] = None,
    bl4: Annotated[
        list[PlatformArg] | None, typer.Option(rich_help_panel="Platform Selection")
    ] = None,
    blps: Annotated[
        list[PlatformArg] | None, typer.Option(rich_help_panel="Platform Selection")
    ] = None,
    ttw: Annotated[
        list[PlatformArg] | None, typer.Option(rich_help_panel="Platform Selection")
    ] = None,
    gdfll: Annotated[
        list[PlatformArg] | None, typer.Option(rich_help_panel="Platform Selection")
    ] = None,
    games: Annotated[
        list[Game] | None,
        typer.Option(
            "--game",
            help="Games you want to query SHiFT keys for",
            rich_help_panel="Global Options",
        ),
    ] = None,
    platforms: Annotated[
        list[Platform] | None,
        typer.Option(
            "--platforms",
            help="Platforms you want to query SHiFT keys for",
            rich_help_panel="Global Options",
        ),
    ] = None,
    user: Annotated[
        str | None,
        typer.Option(
            "--user",
            "-u",
            help="User login (optional, will prompt if not provided)",
            rich_help_panel="Global Options",
        ),
    ] = None,
    password: Annotated[
        str | None,
        typer.Option(
            "--pass",
            "-p",
            help="Password for your login (optional, will prompt if not provided)",
            rich_help_panel="Global Options",
        ),
    ] = None,
    verbose: Annotated[
        bool,
        typer.Option(
            "--verbose",
            "-v",
            help="Enable verbose mode",
            rich_help_panel="Global Options",
        ),
    ] = False,
):
    if "--help" in sys.argv or "-h" in sys.argv:
        # shortcircuit
        return

    if TYPE_CHECKING:
        _silence_unused = (bl1, bl2, bl3, bl4, blps, ttw, gdfll)

    if user:
        settings.USER = user

    if password:
        settings.PASS = SecretStr(password)

    if platforms:
        settings.PLATFORMS = platforms

        if games:
            settings.GAMES = games
            for game in games:
                settings._GAMES_PLATFORM_MAP[game].update(platforms)

    for game in Game:
        value = locals().get(game.name)
        if value is None:
            continue
        if any(v == PlatformArg.all for v in value):  # pyright: ignore[reportAttributeAccessIssue]
            value = list(Platform)
        settings._GAMES_PLATFORM_MAP[game].update(value)

    # Set up logging
    if verbose:
        _L.setLevel(logging.DEBUG)
        _L.debug("Debug mode on")

    if not hasattr(ctx, "__run_command"):
        return

    # Check for first-time usage
    if not settings.COOKIE_FILE.exists():
        typer.echo(LICENSE_TEXT)

    from autoshift.storage import database

    if database.is_closed():
        database.connect()
        run_migrations(database)

    # try logging in first. Does nothing if already logged in
    client.login(
        settings.USER, settings.PASS.get_secret_value() if settings.PASS else None
    )


@app.command("redeem")
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
    elif db_key.redeemed:
        _L.info("You already redeemed that code.")
        return
    redeem(db_key)


@app.command("schedule")
def auto_redeem_codes(
    interval: Annotated[
        int | None,
        typer.Option(
            help="Keep checking for keys every N minutes (minimum 120)",
        ),
    ] = None,
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

    if interval:
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


@app.command("query")
def query():
    query_keys(settings._GAMES_PLATFORM_MAP)


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


click_app = cast(click.Group, typer.main.get_command(app))


def wrap(command: click.Command, cb: click.Command) -> click.Command:
    orig_cb = cast(Callable, command.callback)

    cmd_kwarg_names = {opt.name for opt in command.params}

    cb_kwarg_names = {opt.name for opt in cb.params}

    def new_callback(*args, **kwargs):
        ctx = click.get_current_context()
        setattr(ctx, "__run_command", True)
        cb_kwargs: dict = {}
        cmd_kwargs: dict = {}
        for k, v in kwargs.items():
            if k in cb_kwarg_names:
                cb_kwargs[k] = v
            if k in cmd_kwarg_names:
                cmd_kwargs[k] = v
        if cb.callback:
            cb.callback(**cb_kwargs)
        return orig_cb(*args, **cmd_kwargs)

    command.callback = new_callback
    command.params.extend(reversed(cb.params))
    return command


for name, cmd in click_app.commands.items():
    click_app.commands[name] = wrap(cmd, click_app)


def run():
    click_app()
    client.__save_cookie()


if __name__ == "__main__":
    run()
