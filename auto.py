#!/usr/bin/env python
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
    status = client.redeem(key.key)
    if status in (Status.SUCCESS, Status.REDEEMED):
        query.set_redeemed(key)


query.open_db()

# query new keys
for game in games:
    print("Parsing {:4} keys...".format(game), end="")
    sys.stdout.flush()
    keys = query.get_keys(platform, game, True)
    query.parse_keys(game)
    new_keys = query.get_keys(platform, game, True)
    diff = len(new_keys) - len(keys)
    print("done. ({} new Keys)".format(diff if diff else "no"))
    print(len(keys))

# print("=====")
# print(query.get_special_keys("ps", BL2))
# print("=====")
# print(query.get_golden_keys("ps", BLPS))

keys = query.get_keys(platform, game)
# for k in keys:
#     status = client.redeem(k.key)
#     if status != query.Status.SUCCESS:
#         break
#     query.set_redeemed(k)

status = client.redeem(keys[0].key)
status = client.redeem(keys[1].key)
query.close_db()
