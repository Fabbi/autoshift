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

import pickle
from enum import Enum
from typing import Any, Literal, Optional, Union, cast

import bs4
import requests
from bs4 import BeautifulSoup as BSoup
from requests.models import Response

from common import _L, DIRNAME

base_url = "https://shift.gearboxsoftware.com"


def json_headers(token):
    return {'x-csrf-token': token,
            'x-requested-with': 'XMLHttpRequest'}


# filthy enum hack with auto convert
class Status(Enum):
    NONE = "Something unexpected happened.."
    REDIRECT = "<target location>"
    TRYLATER = "To continue to redeem SHiFT codes, please launch a SHiFT-enabled title first!"
    EXPIRED = "This code expired by now.. ({key.reward})"
    REDEEMED = "Already redeemed {key.reward}"
    SUCCESS = "Redeemed {key.reward}"
    INVALID = "The code `{key.code}` is invalid"
    UNKNOWN = "A unknown Error occured: {msg}"

    def __init__(self, s: str):
        self.msg = s

    @classmethod
    def _missing_(cls, value: str):
        obj = object.__new__(cls)
        obj._value_ = value
        obj.__init__(value)
        cls._value2member_map_[value] = obj # type: ignore # these bindings are wrong..
        return obj

    def __eq__(self, other: Any) -> bool:
        if not isinstance(other, self.__class__):
            return False
        return self._name_ == other._name_

    def __call__(self, new_msg: str):
        new_msg = new_msg.format(msg=new_msg)
        obj = self.__class__(new_msg)
        for k, v in vars(self).items():
            if not hasattr(obj, k):
                setattr(obj, k, v)
        return obj

# windows / unix `getch`
try:
    import msvcrt

    def getch(): # type:ignore
        return str(msvcrt.getch(), "utf8") # type:ignore

    BACKSPACE = 8

except ImportError:
    def getch():
        """Get keypress without echoing"""
        import sys
        import termios  # noqa
        import tty
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            ch = sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return ch

    BACKSPACE = 127


def input_pw(qry):
    """Input Password with * replacing chars"""
    import sys
    print(qry, end="")
    sys.stdout.flush()
    pw = ""
    while True:
        c = getch()
        # C^c
        if ord(c) == 3:
            sys.exit(0)

        # on return
        if c == '\r':
            break

        # backspace
        if ord(c) == BACKSPACE:
            pw = pw[:-1]
        else:
            # ignore non-chars
            if ord(c) < 32:
                continue
            pw += c
        print(f"\r{qry}{'*' * len(pw)}", end=" \b")
        sys.stdout.flush()
    return pw

class ShiftClient:
    def __init__(self, user: str = None, pw: str = None):
        from os import path
        self.client = requests.session()
        self.last_status = Status.NONE
        self.cookie_file = path.join(DIRNAME, "data", ".cookies.save")
        # try to load cookies. Query for login data if not present
        if not self.__load_cookie():
            print("First time usage: Login to your SHiFT account...")
            if not user:
                user = input("Username: ")
            if not pw:
                pw = input_pw("Password: ")

            self.__login(user, pw)
            if self.__save_cookie():
                _L.info("Login Successful")
            else:
                _L.error("Couldn't login. Are your credentials correct?")
                exit(0)

    def __save_cookie(self) -> bool:
        """Make ./data folder if not present"""
        from os import mkdir, path
        if not path.exists(path.dirname(self.cookie_file)):
            mkdir(path.dirname(self.cookie_file))

        """Save cookie for auto login"""
        with open(self.cookie_file, "wb") as f:
            for cookie in self.client.cookies:
                if cookie.name == "si":
                    pickle.dump(self.client.cookies, f)
                    return True
        return False

    def __load_cookie(self) -> bool:
        """Check if there is a saved cookie and load it."""
        from os import path
        if (not path.exists(self.cookie_file)):
            return False
        with open(self.cookie_file, "rb") as f:
            content = f.read()
            if (not content):
                return False
            self.client.cookies.update(pickle.loads(content))
        return True

    def redeem(self, code: str, game: str, platform: str) -> Status:
        found, status_code, form_data = self.__get_redemption_form(code, game, platform)
        # the expired message comes from even wanting to redeem
        if not found:
            if status_code >= 500:
                # entered key was invalid
                status = Status.INVALID
            elif "expired" in form_data:
                status = Status.EXPIRED
            elif "not available" in form_data:
                status = Status.INVALID
            elif "already been redeemed" in form_data:
                status = Status.REDEEMED
            else:
                # unknown
                # _L.error(form_data)
                status = Status.UNKNOWN(str(form_data))
        else:
            # the key is valid and all.
            status = self.__redeem_form(cast(dict[str, str], form_data))

        self.last_status = status
        _L.debug(f"{status}: {status.msg}")
        return status

    def __get_token(self, url_or_reply: Union[str, requests.Response]) -> tuple[int,
                                                                                Optional[str]]:
        """Get CSRF-Token from given URL"""
        if isinstance(url_or_reply, str):
            r = self.client.get(url_or_reply)
            _L.debug(f"{r.request.method} {r.url} {r.status_code}")
        else:
            r = url_or_reply

        soup = BSoup(r.text, "html.parser")
        meta = soup.find("meta", attrs=dict(name="csrf-token"))
        if not meta:
            return r.status_code, None
        return r.status_code, meta["content"]

    def __login(self, user, pw):
        """Login with user/pw"""
        the_url = f"{base_url}/home"
        _, token = self.__get_token(the_url)
        if not token:
            return None
        login_data = {"authenticity_token": token,
                      "user[email]": user,
                      "user[password]": pw}
        headers = {"Referer": the_url}
        r = self.client.post(f"{base_url}/sessions",
                             data=login_data,
                             headers=headers)
        _L.debug(f"{r.request.method} {r.url} {r.status_code}")
        return r

    def __get_redemption_form(self, code: str, game: str, platform: str) -> Union[
            tuple[Literal[False], int, str], tuple[Literal[True], int, dict[str, str]]]:
        """Get Form data for code redemption"""

        the_url = f"{base_url}/code_redemptions/new"
        status_code, token = self.__get_token(the_url)
        if not token:
            _L.debug("no token")
            return False, status_code, "Could not retrieve Token"

        r = self.client.get(f"{base_url}/entitlement_offer_codes?code={code}",
                            headers=json_headers(token))
        _L.debug(f"{r.request.method} {r.url} {r.status_code}")
        soup = BSoup(r.text, "html.parser")
        if not soup.find("form", class_="new_archway_code_redemption"):
            return False, r.status_code, r.text.strip()

        titles = soup.find_all("h2")
        title: bs4.element.Tag = titles[0]
        # some codes work for multiple games and yield multiple buttons for the same platform..
        if (len(titles) > 1):
            for _t in titles:
                if cast(bs4.element.Tag, _t).text == game:
                    title = _t
                    break

        forms = title.find_all_next("form", id="new_archway_code_redemption")
        for form in forms:
            service: bs4.element.Tag = form.find(id="archway_code_redemption_service")
            if platform not in service["value"]:
                continue
            form_data = {inp["name"]: inp["value"]
                         for inp in form.find_all("input")}
            return True, r.status_code, form_data

        return (False, r.status_code,
                "This code is not available for your platform")

    def __get_status(self, alert) -> Status:
        status = Status.NONE
        if "success" in alert.lower():
            status = Status.SUCCESS
        elif "failed" in alert.lower():
            status = Status.REDEEMED

        return status

    def __get_redemption_status(self, r: Response) -> tuple[str, str, str]:
        # return None
        soup = BSoup(r.text, "lxml")
        div = soup.find("div", id="check_redemption_status")
        if div:
            return (div.text.strip(), div["data-url"], div["data-fallback-url"])
        return ("", "", "")

    def __check_redemption_status(self, r: Response) -> Status:
        """Check redemption"""
        import json
        from time import sleep
        if (r.status_code == 302):
            return Status.REDIRECT(r.headers["location"])

        get_status, url, fallback = self.__get_redemption_status(r)
        if get_status:
            _, token = self.__get_token(r)
            cnt = 0
            # follow all redirects
            while True:
                if cnt > 5:
                    return Status.REDIRECT(fallback)
                _L.info(get_status)
                _L.debug(f"get {base_url}/{url}")
                raw_json = self.client.get(f"{base_url}/{url}",
                                           allow_redirects=False,
                                           headers=json_headers(token))
                _L.debug(f"Raw json text: {raw_json.text}")
                data = json.loads(raw_json.text)

                if "text" in data:
                    return self.__get_status(data["text"])
                # wait 500
                sleep(0.5)
                cnt += 1

        return Status.NONE

    def __query_rewards(self):
        """Query reward list"""
        # self.old_rewards
        the_url = f"{base_url}/rewards"
        r = self.client.get(the_url)
        soup = BSoup(r.text, "html.parser")

        # cache all unlocked rewards
        return [el.text
                for el in soup.find_all("div", class_="reward_unlocked")]

    def __redeem_form(self, data: dict[str, str]) -> Status:
        """Redeem a code with given form data"""

        the_url = f"{base_url}/code_redemptions"
        headers = {"Referer": f"{the_url}/new"}
        r = self.client.post(the_url,
                             data=data,
                             headers=headers,
                             allow_redirects=False)
        _L.debug(f"{r.request.method} {r.url} {r.status_code}")
        status = self.__check_redemption_status(r)
        # did we visit /code_redemptions/...... route?
        redemption = False
        # keep following redirects
        while status == Status.REDIRECT:
            if "code_redemptions/" in status.value:
                redemption = True
            _L.debug(f"redirect to '{status.value}'")
            r2 = self.client.get(status.value)
            status = self.__check_redemption_status(r2)

        # workaround for new SHiFT website.
        # it doesn't tell you to launch a "SHiFT-enabled title" anymore
        if status == Status.NONE:
            if redemption:
                status = Status.REDEEMED
            else:
                status = Status.TRYLATER
        return status
