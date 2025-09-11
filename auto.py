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
from __future__ import print_function

import sys
from enum import Enum
from typing import Annotated, Match, cast

import typer
from typer import Typer

from common import _L, DEBUG, DIRNAME, INFO
# from query import BL3
from query import Key, known_games, known_platforms
from shift import ShiftClient, Status

# Create Enums for valid choices
class GameChoice(str, Enum):
    bl1 = "bl1"
    bl2 = "bl2" 
    bl3 = "bl3"
    blps = "blps"
    ttw = "ttw"
    gdfll = "gdfll"

class PlatformChoice(str, Enum):
    steam = "steam"
    epic = "epic"
    psn = "psn"
    xboxlive = "xboxlive"
    stadia = "stadia"

app = Typer(help="AutoSHiFt: Automatically redeem Gearbox SHiFT Codes")
client: ShiftClient = None # type: ignore

LICENSE_TEXT = """\
========================================================================
autoshift  Copyright (C) 2019  Fabian Schweinfurth
This program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.
This is free software, and you are welcome to redistribute it
under certain conditions; see LICENSE for details.
========================================================================
"""


def redeem(key: Key):
    import query
    """Redeem key and set as redeemed if successfull"""

    _L.info(f"Trying to redeem {key.reward} ({key.code})")
    status = client.redeem(key.code, known_games[key.game], key.platform)
    _L.debug(f"Status: {status}")

    # set redeemed status
    if status in (Status.SUCCESS, Status.REDEEMED,
                  Status.EXPIRED, Status.INVALID):
        query.db.set_redeemed(key)

    # notify user
    try:
        # this may fail if there are other `{<something>}` in the string..
        _L.info("  " + status.msg.format(**locals()))
    except:
        _L.info("  " + status.msg)

    return status == Status.SUCCESS


def query_keys(games: list[str], platforms: list[str]):
    """Query new keys for given games and platforms

    Returns dict of dicts of lists with [game][platform] as keys"""
    from itertools import groupby

    import query
    all_keys: dict[str, dict[str, list[Key]]] = {}

    keys = list(query.db.get_keys(None, None))
    # parse all keys
    query.update_keys()
    new_keys = list(query.db.get_keys(None, None))

    diff = len(new_keys) - len(keys)
    _L.info(f"done. ({diff if diff else 'no'} new Keys)")

    _g = lambda key: key.game
    _p = lambda key: key.platform
    for g, g_keys in groupby(sorted(new_keys, key=_g), _g):
        if g not in games:
            continue
        all_keys[g] = {p: [] for p in platforms}
        for platform, p_keys in groupby(sorted(g_keys, key=_p), _p):
            if platform not in platforms and platform != "universal":
                continue

            _ps = [platform]
            if platform == "universal":
                _ps = platforms.copy()

            for p in _ps:
                all_keys[g][p].extend(key.copy().set(platform=p) for key in p_keys)
                # all_keys[g][p] = list(p_keys)

        for p in platforms:
            # count the new keys
            n_golden = sum(int(cast(Match[str], m).group(1) or 1)
                            for m in
                            filter(lambda m:
                                    m  and m.group(1) is not None,
                                    map(lambda key: query.r_golden_keys.match(key.reward),
                                        all_keys[g][p])))

            _L.info(f"You have {n_golden} golden {g.upper()} keys to redeem for {p.upper()}")

    return all_keys


@app.command("redeem")
def redeem_codes(
    games: Annotated[list[GameChoice], typer.Option(
        help="Games you want to query SHiFT keys for"
    )],
    platforms: Annotated[list[PlatformChoice], typer.Option(
        help="Platforms you want to query SHiFT keys for"
    )],
    user: Annotated[str | None, typer.Option(
        "--user", "-u",
        help="User login (optional, will prompt if not provided)"
    )] = None,
    password: Annotated[str | None, typer.Option(
        "--pass", "--password",
        help="Password for your login (optional, will prompt if not provided)"
    )] = None,
    golden: Annotated[bool, typer.Option(
        "--golden",
        help="Only redeem golden keys"
    )] = False,
    non_golden: Annotated[bool, typer.Option(
        "--non-golden",
        help="Only redeem non-golden keys"
    )] = False,
    limit: Annotated[int, typer.Option(
        "--limit",
        help="Max number of golden keys to redeem (default 200, max 255)"
    )] = 200,
    schedule: Annotated[float | None, typer.Option(
        "--schedule", "-s",
        help="Keep checking for keys every N hours (minimum 2)"
    )] = None,
    verbose: Annotated[bool, typer.Option(
        "--verbose", "-v",
        help="Enable verbose mode"
    )] = False,
):
    """Redeem SHiFT codes for specified games and platforms."""
    # Convert enums to strings for compatibility with existing code
    games_str = [game.value for game in games]
    platforms_str = [platform.value for platform in platforms]
    
    # Set up logging
    _L.setLevel(INFO)
    if verbose:
        _L.setLevel(DEBUG)
        _L.debug("Debug mode on")
    
    if schedule and schedule < 2:
        _L.warning(f"Running this tool every {schedule} hours would result in "
                  "too many requests.\n"
                  "Scheduling changed to run every 2 hours!")
        schedule = 2
    
    # Check for first-time usage
    import os
    if not os.path.exists(os.path.join(DIRNAME, "data", ".cookies.save")):
        typer.echo(LICENSE_TEXT)
    
    # Execute main logic
    main(games_str, platforms_str, user, password, golden, non_golden, limit, verbose)
    
    # Handle scheduling
    if schedule:
        hours = int(schedule)
        minutes = int((schedule-hours)*60+1e-5)
        _L.info(f"Scheduling to run every {hours:02}:{minutes:02} hours")
        from apscheduler.schedulers.blocking import BlockingScheduler
        scheduler = BlockingScheduler()
        scheduler.add_job(
            main, "interval", 
            args=(games_str, platforms_str, user, password, golden, non_golden, limit, verbose),
            hours=schedule
        )
        typer.echo(f"Press Ctrl+{'Break' if os.name == 'nt' else 'C'} to exit")
        
        try:
            scheduler.start()
        except (KeyboardInterrupt, SystemExit):
            pass
    
    _L.info("Goodbye.")


def main(games: list[str], platforms: list[str], user: str | None, password: str | None, 
         golden: bool, non_golden: bool, limit: int, verbose: bool):
    global client
    from time import sleep

    import query
    from query import db, r_golden_keys

    with db:
        if not client:
            client = ShiftClient(user, password)

        # query all keys
        all_keys = query_keys(games, platforms)

        # redeem 0 golden keys but only golden??... duh
        if not limit and golden:
            _L.info("Not redeeming anything ...")
            return

        _L.info("Trying to redeem now.")

        # now redeem
        for game in all_keys.keys():
            for platform in all_keys[game].keys():
                t_keys = list(filter(lambda key: not key.redeemed, all_keys[game][platform]))
                for num, key in enumerate(t_keys):

                    if (num and not (num % 15)) or client.last_status == Status.SLOWDOWN:
                        if client.last_status == Status.SLOWDOWN:
                            _L.info("Slowing down a bit..")
                        else:
                            _L.info("Trying to prevent a 'too many requests'-block.")
                        sleep(60)

                    _L.info(f"Key #{num+1}/{len(t_keys)}")
                    num_g_keys = 0  # number of golden keys in this code
                    m = r_golden_keys.match(key.reward)

                    # skip keys we don't want
                    if ((golden and not m) or (non_golden and m)):
                        continue

                    if m:
                        num_g_keys = int(m.group(1) or 1)
                        # skip golden keys if we reached the limit
                        if limit <= 0:
                            continue

                        # skip if this code has too many golden keys
                        if (limit - num_g_keys) < 0:
                            continue

                    redeemed = redeem(key)
                    if redeemed:
                        limit -= num_g_keys
                        _L.info(f"Redeeming another {limit} Keys")
                    else:
                        # don't spam if we reached the hourly limit
                        if client.last_status == Status.TRYLATER:
                            return

        _L.info("No more keys left!")


if __name__ == "__main__":
    app()
