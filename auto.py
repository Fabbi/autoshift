#!/usr/bin/env python
from __future__ import print_function
import sys

import query
from query import BL2, BLPS, PC, PS, XBOX # noqa
# from query import BL3
from shift import ShiftClient, Status
from common import getLogger, INFO, DEBUG, GLOBAL_LVL # noqa

_L = getLogger("Auto")
_L.setLevel(GLOBAL_LVL)
"""
TODO
- argsParser:
  - #keys to redeem
  - game
  - platform
- all messages
"""
# ###### PLATFORM
platforms = [PC, PS, XBOX]
# platform = PC
platform = PS
# platform = XBOX

# ###### GAME
games = [BL2, BLPS]
# game = BL2
game = BLPS

client = ShiftClient()


def redeem(key):
    """Redeem key and set as redeemed if successfull"""
    _L.debug("Trying to redeem {} ({})".format(key.description, key.key))
    status = client.redeem(key.key)
    if status in (Status.SUCCESS, Status.REDEEMED):
        query.set_redeemed(key)
        if status == Status.SUCCESS:
            _L.info("Redeemed {}".format(key.description))
        else:
            _L.info("Already redeemed {}".format(key.description))
        return True
    return False


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
            query.parse_keys(game)

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
    parser.add_argument("--golden",
                        action="store_true",
                        help="Only redeem golden keys")
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

    return parser


def main(args):
    import re
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
                    continue
                num_g_keys = 0  # number of golden keys in this one
                m = g_reg.match(key.description)

                # skip non-golden keys if we're told to
                if (args.golden and not m):
                    continue

                if m:
                    num_g_keys = int(m.group(1))
                    # skip golden Keys if we reached the limit
                    if args.limit <= 0:
                        continue

                    # skip if this key has too many golden keys
                    if (args.limit - num_g_keys) < 0:
                        continue

                # redeemed = True
                redeemed = redeem(key)
                if redeemed:
                    args.limit -= num_g_keys
                else:
                    # don't spam if we reached the hourly limit
                    if client.last_status == Status.TRYLATER:
                        _L.info("Please launch a SHiFT-enabled title or wait 1 hour.") # noqa
                        return

    query.close_db()


if __name__ == "__main__":
    # build argument parser
    parser = setup_argparser()
    args = parser.parse_args()

    # always execute at least once
    main(args)

    # scheduling will start after first trigger (so in an hour..)
    if args.schedule:
        _L.info("Scheduling to run once an hour")
        import os
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
