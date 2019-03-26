#!/usr/bin/env python
import pickle
import requests
from bs4 import BeautifulSoup as BSoup

base_url = "https://shift.gearboxsoftware.com"

DEBUG = True


def Status(n):
    els = ["NONE", "REDIRECT", "TRYLATER",
           "EXPIRED", "REDEEMED",
           "SUCCESS", "UNKNOWN"]
    return els[n]
for i in range(7): # noqa
    setattr(Status, Status(i), i)


def dprint(*msg):
    if DEBUG:
        print("[DEBUG]: ", end="")
        print(*msg)


def getch():
    """Get keypress without echoing"""
    import termios
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


def input_pw(qry):
    import sys
    print(qry, end="")
    sys.stdout.flush()
    pw = ""
    while True:
        c = getch()
        # C^c
        if ord(c) == 3:
            sys.exit(0)

        if c == '\r':
            break

        # ignore non-chars
        if ord(c) < 32:
            continue

        if ord(c) == 127:
            pw = pw[:-1]
            print("\r{}{}".format(qry, "*" * len(pw)), end=" \b")
        else:
            pw += c
            print("\r{}{}".format(qry, "*" * len(pw)), end="")
        sys.stdout.flush()
    return pw


class ShiftClient:
    def __init__(self):
        from os import path
        self.client = requests.session()
        filepath = path.realpath(__file__)
        dirname = path.dirname(filepath)
        self.cookie_file = path.join(dirname, ".cookies.save")
        # try to load cookies. Query for login data if not present
        if not self.__load_cookie():
            print("First time usage: You need to login...")
            user = input("Username: ")
            pw = input_pw("Password: ")
            self.__login(user, pw)
            self.__save_cookie()

    def redeem(self, code):
        form_data = self.__get_redemption_form(code)
        if not form_data:
            return Status.UNKNOWN
        dprint("trying to redeem {}".format(code))
        status, result = self.__redeem_form(form_data)
        dprint(Status(status), result)
        return status

    def __save_cookie(self):
        """Save cookie for auto login"""
        with open(self.cookie_file, "wb") as f:
            for cookie in self.client.cookies:
                if cookie.name == "si":
                    pickle.dump(self.client.cookies, f)
                    break

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
            return None
        return meta["content"]

    def __login(self, user, pw):
        """Login with user/pw"""
        the_url = "{}/home".format(base_url)
        token = self.__get_token(the_url)
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
        token = self.__get_token(the_url)
        if not token:
            dprint("no token")
            return None
        headers = {'x-csrf-token': token,
                   'x-requested-with': 'XMLHttpRequest'}
        r = self.client.get(f"{base_url}/entitlement_offer_codes?code={code}",
                            headers=headers)
        soup = BSoup(r.text, "html.parser")
        if not soup.find("form", class_="new_archway_code_redemption"):
            print("Error: {}".format(r.text.strip()))
            return {}

        inp = soup.find("input", attrs=dict(name="authenticity_token"))
        form_code = soup.find(id="archway_code_redemption_code")["value"]
        check = soup.find(id="archway_code_redemption_check")["value"]
        service = soup.find(id="archway_code_redemption_service")["value"]

        return {"authenticity_token": inp["value"],
                "archway_code_redemption[code]": form_code,
                "archway_code_redemption[check]": check,
                "archway_code_redemption[service]": service}

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
            dprint(alert)
            return status, alert

        get_status, url = self.__get_status(r)
        if get_status:
            print(get_status)
            # wait 500
            sleep(0.5)
            r2 = self.client.get("{}/{}".format(base_url, url),
                                 allow_redirects=False)
            return self.__check_redemption_status(r2)
        return Status.NONE, None

    def __redeem_form(self, data):
        """Redeem a code with given form data"""
        the_url = "{}/code_redemptions".format(base_url)
        headers = {"Referer": "{}/new".format(the_url)}
        r = self.client.post(the_url,
                             data=data,
                             headers=headers,
                             allow_redirects=False)
        status, redirect = self.__check_redemption_status(r)
        # if status == Status.REDIRECT:
        while status == Status.REDIRECT:
            dprint("redirect to '{}'".format(redirect))
            r2 = self.client.get(redirect)
            # import ipdb; ipdb.set_trace()
            # print(r2.text)
            status, redirect = self.__check_redemption_status(r2)
        return status, redirect


# client = ShiftClient()
# import ipdb; ipdb.set_trace()
# client.redeem("C5WTB-K5R59-HSBW3-H3K3T-KCC6J")
