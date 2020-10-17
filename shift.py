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
import requests
from bs4 import BeautifulSoup as BSoup

from common import _L, DIRNAME

base_url = "https://shift.gearboxsoftware.com"


def json_headers(token):
    return {'x-csrf-token': token,
            'x-requested-with': 'XMLHttpRequest'}


# filthy enum hack with auto convert
__els = ["NONE", "REDIRECT", "TRYLATER",
         "EXPIRED", "REDEEMED",
         "SUCCESS", "INVALID", "UNKNOWN"]
def Status(n): # noqa
    return __els[n]


Status.NONE = 0
Status.REDIRECT = 1
Status.TRYLATER = 2
Status.EXPIRED = 3
Status.REDEEMED = 4
Status.SUCCESS = 5
Status.INVALID = 6
Status.UNKNOWN = 7
# for i in range(len(__els)): # noqa
#     setattr(Status, Status(i), i)


# windows / unix `getch`
try:
    import msvcrt # noqa

    def getch():
        return str(msvcrt.getch(), "utf8")

    BACKSPACE = 8

except ImportError:
    def getch():
        """Get keypress without echoing"""
        import termios # noqa
        import tty
        import sys
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
        print("\r{}{}".format(qry, "*" * len(pw)), end=" \b")
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

    def redeem(self, code, platform):
        found, status_code, form_data = self.__get_redemption_form(code,
                                                                   platform)
        # the expired message comes from even wanting to redeem
        if not found:
            # entered key was invalid
            if status_code == 500:
                return Status.INVALID
            # entered key expired by now
            if "expired" in form_data:
                return Status.EXPIRED
            if "not available" in form_data:
                return Status.INVALID
            # unknown
            _L.error(form_data)
            return Status.UNKNOWN

        # the key is valid and all.
        status, result = self.__redeem_form(form_data)
        self.last_status = status
        _L.debug("{}: {}".format(Status(status), result))
        return status

    def __save_cookie(self):
        """Make ./data folder if not present"""
        from os import path, mkdir
        if not path.exists(path.dirname(self.cookie_file)):
            mkdir(path.dirname(self.cookie_file))

        """Save cookie for auto login"""
        with open(self.cookie_file, "wb") as f:
            for cookie in self.client.cookies:
                if cookie.name == "si":
                    pickle.dump(self.client.cookies, f)
                    return True
        return False

    def __load_cookie(self):
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

    def __get_token(self, url_or_reply):
        """Get CSRF-Token from given URL"""
        if type(url_or_reply) == str:
            r = self.client.get(url_or_reply)
            _L.debug("{} {} {}".format(r.request.method, r.url, r.status_code))
        else:
            r = url_or_reply

        soup = BSoup(r.text, "html.parser")
        meta = soup.find("meta", attrs=dict(name="csrf-token"))
        if not meta:
            return r.status_code, None
        return r.status_code, meta["content"]

    def __login(self, user, pw):
        """Login with user/pw"""
        the_url = "{}/home".format(base_url)
        status_code, token = self.__get_token(the_url)
        if not token:
            return None
        login_data = {"authenticity_token": token,
                      "user[email]": user,
                      "user[password]": pw}
        headers = {"Referer": the_url}
        r = self.client.post("{}/sessions".format(base_url),
                             data=login_data,
                             headers=headers)
        _L.debug("{} {} {}".format(r.request.method, r.url, r.status_code))
        return r

    def __get_redemption_form(self, code, platform):
        """Get Form data for code redemption"""
        the_url = "{}/code_redemptions/new".format(base_url)
        status_code, token = self.__get_token(the_url)
        if not token:
            _L.debug("no token")
            return False, status_code, "Could not retrieve Token"

        r = self.client.get("{base_url}/entitlement_offer_codes?code={code}"
                            .format(base_url=base_url, **locals()),
                            headers=json_headers(token))

        _L.debug("{} {} {}".format(r.request.method, r.url, r.status_code))
        soup = BSoup(r.text, "html.parser")
        if not soup.find("form", class_="new_archway_code_redemption"):
            return False, r.status_code, r.text.strip()

        inp = soup.find_all("input", attrs=dict(name="authenticity_token"))
        form_code = soup.find_all(id="archway_code_redemption_code")
        check = soup.find_all(id="archway_code_redemption_check")
        service = soup.find_all(id="archway_code_redemption_service")

        ind = None
        for i, s in enumerate(service):
            if platform in s["value"]:
                ind = i
                break
        if (ind is None):
            return (False, r.status_code,
                    "This code is not available for your platform")

        form_data = {"authenticity_token": inp[ind]["value"],
                     "archway_code_redemption[code]": form_code[ind]["value"],
                     "archway_code_redemption[check]": check[ind]["value"],
                     "archway_code_redemption[service]": service[ind]["value"]}
        return True, r.status_code, form_data

    def __get_status(self, alert):
        status = Status.NONE
        if "success" in alert.lower():
            status = Status.SUCCESS
        elif "failed" in alert.lower():
            status = Status.REDEEMED

        return status, alert

    def __get_redemption_status(self, r):
        # return None
        soup = BSoup(r.text, "lxml")
        div = soup.find("div", id="check_redemption_status")
        ret = [None, None, None]
        if div:
            ret[0] = div.text.strip()
            ret[1] = div["data-url"]
            ret[2] = div["data-fallback-url"]
        return ret

    def __check_redemption_status(self, r):
        """Check redemption"""
        import json
        from time import sleep
        if (r.status_code == 302):
            return Status.REDIRECT, r.headers["location"]

        get_status, url, fallback = self.__get_redemption_status(r)
        if get_status:
            status_code, token = self.__get_token(r)
            cnt = 0
            while True:
                if cnt > 5:
                    return Status.REDIRECT, fallback
                _L.info(get_status)
                # wait 500
                _L.debug("get " + "{}/{}".format(base_url, url))
                raw_json = self.client.get("{}/{}".format(base_url, url),
                                           allow_redirects=False,
                                           headers=json_headers(token))
                data = json.loads(raw_json.text)

                if "text" in data:
                    get_status = self.__get_status(data["text"])
                    return get_status

                sleep(0.5)
                cnt += 1

        return Status.NONE, None

    def __query_rewards(self):
        """Query reward list"""
        # self.old_rewards
        the_url = "{}/rewards".format(base_url)
        r = self.client.get(the_url)
        soup = BSoup(r.text, "html.parser")

        # cache all unlocked rewards
        return [el.text
                for el in soup.find_all("div", class_="reward_unlocked")]

    def __redeem_form(self, data):
        """Redeem a code with given form data"""
        # cache old reward list
        self.old_rewards = self.__query_rewards()

        the_url = "{}/code_redemptions".format(base_url)
        headers = {"Referer": "{}/new".format(the_url)}
        r = self.client.post(the_url,
                             data=data,
                             headers=headers,
                             allow_redirects=False)
        _L.debug("{} {} {}".format(r.request.method, r.url, r.status_code))
        status, redirect = self.__check_redemption_status(r)
        # did we visit /code_redemptions/...... route?
        redemption = False
        # keep following redirects
        while status == Status.REDIRECT:
            if "code_redemptions/" in redirect:
                redemption = True
            _L.debug("redirect to '{}'".format(redirect))
            r2 = self.client.get(redirect)
            status, redirect = self.__check_redemption_status(r2)

        # workaround for new SHiFT website.
        # it doesn't tell you to launch a "SHiFT-enabled title" anymore
        if status == Status.NONE:
            if redemption:
                status = Status.REDEEMED
                redirect = "Already Redeemed"
            else:
                status = Status.TRYLATER
                redirect = "To continue to redeem SHiFT codes, please launch a SHiFT-enabled title first!" # noqa
        return status, redirect
