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
import getpass
import pickle
import requests
from bs4 import BeautifulSoup as BSoup

from common import _L, DIRNAME

base_url = "https://shift.gearboxsoftware.com"


# filthy enum hack with auto convert
__els = ["NONE", "REDIRECT", "TRYLATER",
         "EXPIRED", "REDEEMED",
         "SUCCESS", "INVALID", "UNKNOWN"]
def Status(n): # noqa
    return __els[n]
for i in range(len(__els)): # noqa
    setattr(Status, Status(i), i)


# windows / unix `getch`
try:
    import msvcrt
    getch = msvcrt.getch
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


class ShiftClient:
    def __init__(self):
        from os import path
        self.client = requests.session()
        self.last_status = Status.NONE
        self.cookie_file = path.join(DIRNAME, ".cookies.save")
        # try to load cookies. Query for login data if not present
        if not self.__load_cookie():
            print("First time usage: You need to login...")
            user = input("Username: ")
            pw = getpass.getpass("Password: ")
            self.__login(user, pw)
            self.__save_cookie()

    def redeem(self, code):
        found, status_code, form_data = self.__get_redemption_form(code)
        # the expired message comes from even wanting to redeem
        if not found:
            # entered key was invalid
            if status_code == 500:
                return Status.INVALID
            # entered key expired by now
            if "expired" in form_data:
                return Status.EXPIRED
            # unknown
            _L.error(form_data)
            return Status.UNKNOWN

        # the key is valid and all.
        status, result = self.__redeem_form(form_data)
        self.last_status = status
        _L.debug("{}: {}".format(Status(status), result))
        return status

    def __save_cookie(self):
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
        if not path.exists(self.cookie_file):
            return False
        with open(self.cookie_file, "rb") as f:
            self.client.cookies.update(pickle.load(f))
        return True

    def __get_token(self, url):
        """Get CSRF-Token from given URL"""
        r = self.client.get(url)
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
        return r

    def __get_redemption_form(self, code):
        """Get Form data for code redemption"""
        the_url = "{}/code_redemptions/new".format(base_url)
        status_code, token = self.__get_token(the_url)
        if not token:
            _L.debug("no token")
            return False, status_code, "Could not retrieve Token"

        headers = {'x-csrf-token': token,
                   'x-requested-with': 'XMLHttpRequest'}
        r = self.client.get("{base_url}/entitlement_offer_codes?code={code}"
                            .format(base_url=base_url, **locals()),
                            headers=headers)
        soup = BSoup(r.text, "html.parser")
        if not soup.find("form", class_="new_archway_code_redemption"):
            return False, r.status_code, r.text.strip()

        inp = soup.find("input", attrs=dict(name="authenticity_token"))
        form_code = soup.find(id="archway_code_redemption_code")["value"]
        check = soup.find(id="archway_code_redemption_check")["value"]
        service = soup.find(id="archway_code_redemption_service")["value"]

        form_data = {"authenticity_token": inp["value"],
                     "archway_code_redemption[code]": form_code,
                     "archway_code_redemption[check]": check,
                     "archway_code_redemption[service]": service}
        return True, r.status_code, form_data

    def __get_alert(self, r):
        """Get Alert Answer after redemption"""
        soup = BSoup(r.text, "html.parser")
        notice = soup.find("div", class_="notice")
        if not notice:
            return Status.NONE, None

        alert = notice.text.strip()

        status = Status.NONE
        if "to continue to redeem" in alert.lower():
            status = Status.TRYLATER
        elif "expired" in alert.lower():
            status = Status.EXPIRED
        elif "success" in alert.lower():
            status = Status.SUCCESS
        elif "failed" in alert.lower():
            status = Status.REDEEMED

        return status, alert

    def __get_status(self, r):
        # return None
        soup = BSoup(r.text, "lxml")
        div = soup.find("div", id="check_redemption_status")
        ret = [None, None]
        if div:
            ret[0] = div.text.strip()
            ret[1] = div["data-url"]
        return ret

    def __check_redemption_status(self, r):
        """Check redemption"""
        from time import sleep
        if (r.status_code == 302):
            return Status.REDIRECT, r.headers["location"]

        status, alert = self.__get_alert(r)
        if alert:
            _L.debug(alert)
            return status, alert

        get_status, url = self.__get_status(r)
        if get_status:
            _L.info(get_status)
            # wait 500
            sleep(0.5)
            r2 = self.client.get("{}/{}".format(base_url, url),
                                 allow_redirects=False)
            return self.__check_redemption_status(r2)

        # workaround for new SHiFT website
        # it doesn't tell you whether your redemption was successful or not
        new_rewards = self.__query_rewards()
        if (new_rewards != self.old_rewards):
            return Status.SUCCESS, ""
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
        status, redirect = self.__check_redemption_status(r)
        # did we visit /code_redemptions/...... route?
        redemption = False
        # keep following redirects
        while status == Status.REDIRECT:
            if "code_redemptions" in redirect:
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
