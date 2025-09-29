# from collections import defaultdict
# from collections.abc import Callable

# import httpx
# from bs4 import BeautifulSoup as BSoup

# from autoshift.common import Game, Platform
# from autoshift.models import Key
# import re

# KEY = re.compile(r"((?:\w{5}-){4}\w{5})")

# type KeyCollector = Callable[[list[Game], list[Platform]], list[Key]]


# collectors: dict[Game, list[KeyCollector]] = defaultdict(list)


# def register(games: list[Game]):
#     def wrapper(f: KeyCollector):
#         for game in games:
#             collectors[game].append(f)
#         return f

#     return wrapper


# @register(games=[Game.bl4])
# def collect_ign_bl4(_games: list[Game], _platforms: list[Platform]) -> list[Key]:
#     url = "https://www.ign.com/wikis/borderlands-4/Borderlands_4_SHiFT_Codes"
#     client = httpx.Client(follow_redirects=True)

#     response = client.get(url)
#     soup = BSoup(response.text, "lxml")

#     code_tags = soup.find_all("code", class_="copy-the-code-target")

#     for tag in code_tags:
#         td = tag.find_parent("td")
#         siblings = td.find_previous_siblings("td")

#     return []
