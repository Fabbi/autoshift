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

import query
from query import games, platforms # noqa
# from query import BL3
from shift import ShiftClient, Status
from common import _L, INFO, DEBUG, DIRNAME


client = None

LICENSE_TEXT = """\
========================================================================
autoshift  Copyright (C) 2019  Fabian Schweinfurth
This program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.
This is free software, and you are welcome to redistribute it
under certain conditions; see LICENSE for details.
========================================================================
"""


def redeem(key, platform):
    """Redeem key and set as redeemed if successfull"""
    messages = {
        Status.SUCCESS: "Redeemed {key.description}",
        Status.EXPIRED: "This code expired by now.. ({key.description})",
        Status.REDEEMED: "Already redeemed {key.description}",
        Status.INVALID: "The code `{key.key}` is invalid",
        Status.TRYLATER: "Please launch a SHiFT-enabled title or wait 1 hour.",
        Status.UNKNOWN: "A unknown Error occured",
        Status.NONE: "Something unexpected happened.."
    }

    _L.info("Trying to redeem {} ({})".format(key.description, key.key))
    status = client.redeem(key.key, platform)
    _L.debug("Status: {}".format(Status(status)))

    # set redeemed status
    if status in (Status.SUCCESS, Status.REDEEMED,
                  Status.EXPIRED, Status.INVALID):
        query.set_redeemed(key)

    # notify user
    _L.info(messages[status].format(**locals()))

    return status == Status.SUCCESS


def query_keys(games, platforms):
    """Query new keys for given games and platforms

    Returns dict of dicts of lists with [game][platform] as keys"""
    all_keys = {}
    # query new keys
    for game in games:
        all_keys[game] = {}
        for platform in platforms:
            print("Parsing {:4} keys...".format(game.upper()), end="")
            sys.stdout.flush()
            keys = query.get_keys(platform, game, True)

            # parse all keys
            query.parse_keys(game, platform)

            new_keys = query.get_keys(platform, game, True)
            all_keys[game][platform] = new_keys

            # count the new keys
            diff = len(new_keys) - len(keys)
            print("done. ({} new Keys)".format(diff if diff else "no"))
            n_golden, g_keys = query.get_golden_keys(platform, game)
            print("You have {} golden {} keys to redeem for {}"
                  .format(n_golden, game.upper(), platform.upper()))
    return all_keys


def setup_argparser():
    import argparse
    import textwrap
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("-u", "--user",
                        default=None,
                        help=("User login you want to use "
                              "(optional. You will be prompted to enter your "
                              " credentials if you didn't specify them here)"))
    parser.add_argument("-p", "--pass",
                        help=("Password for your login. "
                              "(optional. You will be prompted to enter your "
                              " credentials if you didn't specify them here)"))
    parser.add_argument("--golden",
                        action="store_true",
                        help="Only redeem golden keys")
    parser.add_argument("--non-golden", dest="non_golden",
                        action="store_true",
                        help="Only redeem non-golden keys")
    parser.add_argument("--games",
                        type=str, required=True,
                        choices=games, nargs="+",
                        help=("Games you want to query SHiFT keys for"))
    parser.add_argument("--platforms",
                        type=str, required=True,
                        choices=platforms, nargs="+",
                        help=("Platforms you want to query SHiFT keys for"))
    parser.add_argument("--limit",
                        type=int, default=200,
                        help=textwrap.dedent("""\
                        Max number of golden Keys you want to redeem.
                        (default 200)
                        NOTE: You can only have 255 keys at any given time!""")) # noqa
    parser.add_argument("--schedule",
                        action="store_true",
                        help="Keep checking for keys and redeeming every hour")
    parser.add_argument("-v", dest="verbose",
                        action="store_true",
                        help="Verbose mode")

    return parser


def main(args):
    global client
    import re

    if not client:
        client = ShiftClient(args.user, args.pw)

    g_reg = re.compile(r"^(\d+).*gold.*", re.I)

    query.open_db()
    # query all keys
    all_keys = query_keys(args.games, args.platforms)

    # redeem 0 golden keys but only golden??... duh
    if not args.limit and args.golden:
        _L.info("Not redeeming anything ...")
        return

    # now redeem
    for game in all_keys.keys():
        for platform in all_keys[game].keys():
            t_keys = all_keys[game][platform]
            for key in t_keys:
                if key.redeemed:
                    # we could query only unredeemed keys
                    # but we have them already so it doesn't matter
                    continue
                num_g_keys = 0  # number of golden keys in this code
                m = g_reg.match(key.description)

                # skip keys we don't want
                if ((args.golden and not m) or (args.non_golden and m)):
                    continue

                if m:
                    num_g_keys = int(m.group(1))
                    # skip golden keys if we reached the limit
                    if args.limit <= 0:
                        continue

                    # skip if this code has too many golden keys
                    if (args.limit - num_g_keys) < 0:
                        continue

                redeemed = redeem(key, platform)
                if redeemed:
                    args.limit -= num_g_keys
                    _L.info("Redeeming another {} Keys".format(args.limit))
                else:
                    # don't spam if we reached the hourly limit
                    if client.last_status == Status.TRYLATER:
                        return

    query.close_db()


if __name__ == "__main__":
    import os
    # only print license text on first use
    if not os.path.exists(os.path.join(DIRNAME, "data", ".cookies.save")):
        print(LICENSE_TEXT)

    # build argument parser
    parser = setup_argparser()
    args = parser.parse_args()

    args.pw = getattr(args, "pass")

    _L.setLevel(INFO)
    if args.verbose:
        _L.setLevel(DEBUG)
        _L.debug("Debug mode on")

    # always execute at least once
    main(args)

    # scheduling will start after first trigger (so in an hour..)
    if args.schedule:
        _L.info("Scheduling to run once an hour")
        from apscheduler.schedulers.blocking import BlockingScheduler
        scheduler = BlockingScheduler()
        # fire every 1h5m
        #  (5min safe margin because it somtimes fires a few seconds too early)
        scheduler.add_job(main, "interval", args=(args,), hours=1, minutes=5)
        print('Press Ctrl+{} to exit'.format('Break' if os.name == 'nt'
                                             else 'C'))

        try:
            scheduler.start()
        except (KeyboardInterrupt, SystemExit):
            pass
