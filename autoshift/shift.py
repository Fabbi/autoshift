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

import pickle
import time
from enum import Enum
from typing import Any, Literal

import httpx
import typer
from bs4 import BeautifulSoup as BSoup
from bs4 import Tag

from autoshift.common import _L, settings
from autoshift.models import Key

base_url = "https://shift.gearboxsoftware.com"


def json_headers(token: str) -> dict[str, str]:
    return {"x-csrf-token": token, "x-requested-with": "XMLHttpRequest"}


# filthy enum hack with auto convert
class Status(Enum):
    NONE = "Something unexpected happened.."
    REDIRECT = "<target location>"
    TRYLATER = (
        "To continue to redeem SHiFT codes, please launch a SHiFT-enabled title first!"
    )
    EXPIRED = "This code expired by now.. ({key.reward})"
    REDEEMED = "Already redeemed {key.reward}"
    SUCCESS = "Redeemed {key.reward}"
    INVALID = "The code `{key.code}` is invalid"
    SLOWDOWN = "Too many requests"
    UNKNOWN = "An unknown Error occured: {msg}"

    def __init__(self, s: str) -> None:
        self.msg = s

    @classmethod
    def _missing_(cls, value: object):
        obj = object.__new__(cls)
        obj._value_ = value
        obj.__init__(str(value))
        cls._value2member_map_[value] = obj  # type: ignore # these bindings are wrong..
        return obj

    def __eq__(self, other: Any) -> bool:
        if not isinstance(other, self.__class__):
            return False
        return self._name_ == other._name_

    def __call__(self, new_msg: str) -> "Status":
        if "{msg}" in new_msg:
            new_msg = new_msg.format(msg=new_msg)
        obj = self.__class__(new_msg)
        for k, v in vars(self).items():
            if not hasattr(obj, k):
                setattr(obj, k, v)
        return obj


class ShiftClient:
    logged_in: bool = False

    def __init__(self):
        # try to load cookies. Query for login data if not present
        self.cookies = self.__load_cookie()
        self.client = httpx.Client(follow_redirects=True, cookies=self.cookies)

    def login(self, user: str | None = None, pw: str | None = None):
        if self.cookies:
            self.logged_in = self.check_login()
        if self.logged_in:
            return True
        typer.echo("Login to your SHiFT account...")
        if not user:
            user = typer.prompt("Username")
        if not pw:
            pw = typer.prompt("Password", hide_input=True)

        if user and pw:
            self.__login(user, pw)
        if self.__save_cookie():
            _L.info("Login Successful")
            self.logged_in = True
        else:
            _L.error("Couldn't login. Are your credentials correct?")
            exit(0)

    def check_login(self) -> bool:
        response = self.client.get(f"{base_url}/rewards")
        return response.status_code == 200 and "Sign Out" in response.text

    def __save_cookie(self) -> bool:
        """Make ./data folder if not present"""
        if not settings.COOKIE_FILE.parent.exists():
            settings.COOKIE_FILE.parent.mkdir(parents=True, exist_ok=True)

        self.client.cookies.jar.clear_expired_cookies()

        """Save cookie for auto login"""
        with settings.COOKIE_FILE.open("wb") as f:
            if "si" in self.client.cookies:
                pickle.dump(self.client.cookies.jar._cookies, f)  # pyright: ignore[reportAttributeAccessIssue]
                return True
        return False

    def __load_cookie(self) -> httpx.Cookies | None:
        """Check if there is a saved cookie and load it."""
        if not settings.COOKIE_FILE.exists() or settings.COOKIE_FILE.stat().st_size == 0:
            return None
        try:
            cookies = httpx.Cookies()
            with settings.COOKIE_FILE.open("rb") as f:
                jar_cookies = pickle.load(f)
                for domain, pc in jar_cookies.items():
                    for path, c in pc.items():
                        for k, v in c.items():
                            cookies.set(k, v.value, domain=domain, path=path)
            return cookies
        except Exception:
            _L.error("Could not load cookies. Re-login required")
            return None

    def redeem(self, key: Key) -> Status:
        from time import sleep

        retry = True
        status = Status.NONE
        game_longname = key.game.long_name

        while retry:
            found, status_code, form_data = self.__get_redemption_form(
                key.code, game_longname, key.platform
            )
            # the expired message comes from even wanting to redeem
            if not found:
                if status_code >= 500:
                    # entered key was invalid
                    status = Status.INVALID
                elif status_code == 429:
                    status = Status.SLOWDOWN
                elif "expired" in form_data:
                    status = Status.EXPIRED
                elif "not available" in form_data:
                    status = Status.INVALID
                elif "does not exist" in form_data:
                    status = Status.INVALID
                elif "already been redeemed" in form_data:
                    status = Status.REDEEMED
                else:
                    # unknown
                    # _L.error(form_data)
                    status = Status.UNKNOWN(str(form_data))
            else:
                # the key is valid and all.
                if isinstance(form_data, dict):
                    status = self.__redeem_form(form_data)
                else:
                    status = Status.UNKNOWN(str(form_data))

            if status == Status.SLOWDOWN:
                sleep(60)
            else:
                retry = False

            key.redeemed = status in (Status.SUCCESS, Status.REDEEMED)
            key.expired = status in (Status.EXPIRED, Status.INVALID)
            key.save()

            if key.expired:
                # set all keys with the same code as expired
                (Key.update(expired=True).where(Key.code == key.code).execute())

        return status

    def __get_token(self, url_or_reply: str | httpx.Response) -> tuple[int, str | None]:
        """Get CSRF-Token from given URL"""
        if isinstance(url_or_reply, str):
            r = self.client.get(url_or_reply)
        else:
            r = url_or_reply

        soup = BSoup(r.text, "html.parser")
        meta = soup.find("meta", attrs=dict(name="csrf-token"))
        if not isinstance(meta, Tag):
            return r.status_code, None
        return r.status_code, str(meta.get("content", ""))

    def __login(self, user: str, pw: str) -> httpx.Response | None:
        """Login with user/pw"""
        the_url = f"{base_url}/home"
        _, token = self.__get_token(the_url)
        if not token:
            return None
        login_data = {
            "authenticity_token": token,
            "user[email]": user,
            "user[password]": pw,
        }
        headers = {"Referer": the_url}
        r = self.client.post(f"{base_url}/sessions", data=login_data, headers=headers)
        _L.debug(f"{r.request.method} {r.url} {r.status_code}")
        return r

    def __get_redemption_form(
        self, code: str, game: str | None, platform: str
    ) -> tuple[Literal[False], int, str] | tuple[Literal[True], int, dict[str, str]]:
        """Get Form data for code redemption"""

        the_url = f"{base_url}/rewards"
        status_code, token = self.__get_token(the_url)
        if not token:
            _L.debug("no token")
            return False, status_code, "Could not retrieve Token"

        r = self.client.get(
            f"{base_url}/entitlement_offer_codes?code={code}", headers=json_headers(token)
        )

        if r.status_code != 200:
            return False, r.status_code, str(r.status_code)

        soup = BSoup(r.text, "html.parser")
        if not soup.find("form", class_="new_archway_code_redemption"):
            return False, r.status_code, r.text.strip()

        titles = soup.find_all("h2")
        title = titles[0]
        # some codes work for multiple games and yield multiple buttons for the same platform..
        if len(titles) > 1:
            for _t in titles:
                if isinstance(_t, Tag) and _t.text == game:
                    title = _t
                    break

        if not isinstance(title, Tag):
            return (False, r.status_code, "Could not find title tag")

        forms = title.find_all_next("form", id="new_archway_code_redemption")
        for form in forms:
            if not isinstance(form, Tag):
                continue
            service = form.find(id="archway_code_redemption_service")
            if isinstance(service, Tag) and platform in str(service.get("value", "")):
                inputs = form.find_all("input")
                form_data = {}
                for inp in inputs:
                    if isinstance(inp, Tag):
                        name = inp.get("name")
                        value = inp.get("value")
                        if name:
                            form_data[str(name)] = str(value or "")
                return True, r.status_code, form_data

        return (False, r.status_code, "This code is not available for your platform")

    def __get_status(self, alert: str) -> Status:
        status = Status.NONE
        lower = alert.lower()
        if "success" in lower:
            status = Status.SUCCESS
        elif "failed" in lower or "been redeemed" in lower:
            status = Status.REDEEMED

        return status

    def __get_redemption_status(self, r: httpx.Response) -> tuple[str, str, str]:
        # return None
        soup = BSoup(r.text, "lxml")
        div = soup.find("div", id="check_redemption_status")
        if not div:
            div = soup.find("div", class_="alert notice")
        if isinstance(div, Tag):
            return (
                div.text.strip(),
                str(div.get("data-url", "")),
                str(div.get("data-fallback-url", "")),
            )
        return ("", "", "")

    def __check_redemption_status(self, r: httpx.Response) -> Status:
        """Check redemption"""
        import json
        from time import sleep

        if r.status_code == 302:
            return Status.REDIRECT(r.headers["location"])

        get_status, url, fallback = self.__get_redemption_status(r)
        if get_status:
            if not url:
                return self.__get_status(get_status)

            _, token = self.__get_token(r)
            cnt = 0
            # follow all redirects
            while True:
                if cnt > 5:
                    return Status.REDIRECT(fallback)
                _L.info(get_status)
                _L.debug(f"get {base_url}/{url}")
                raw_json = self.client.get(
                    f"{base_url}/{url}",
                    follow_redirects=False,
                    headers=json_headers(token or ""),
                )
                _L.debug(f"Raw json text: {raw_json.text}")
                data = json.loads(raw_json.text)

                if "text" in data:
                    return self.__get_status(data["text"])
                # wait 500
                sleep(0.5)
                cnt += 1

        return Status.NONE

    def _query_rewards(self) -> list[str]:
        """Query reward list"""
        # self.old_rewards
        the_url = f"{base_url}/rewards"
        r = self.client.get(the_url)
        soup = BSoup(r.text, "html.parser")

        # cache all unlocked rewards
        return [el.text for el in soup.find_all("div", class_="reward_unlocked")]

    def __redeem_form(self, data: dict[str, str]) -> Status:
        """Redeem a code with given form data"""

        the_url = f"{base_url}/code_redemptions"
        headers = {"Referer": f"{base_url}/rewards"}
        response = self.client.post(
            the_url, data=data, headers=headers, follow_redirects=False
        )
        _L.debug(f"{response.request.method} {response.url} {response.status_code}")
        status = self.__check_redemption_status(response)
        # did we visit /code_redemptions/...... route?
        redemption = False
        # keep following redirects
        while status == Status.REDIRECT:
            if "code_redemptions/" in status.value:
                redemption = True
                while True:
                    response2 = self.client.get(
                        status.value,
                        headers={
                            "referer": status.value,
                            "x-requested-with": "XMLHttpRequest",
                            "accept": "application/json",
                        },
                    )
                    json_data = response2.json()
                    if json_data.get("in_progress", False):
                        time.sleep(0.5)
                        continue
                    try:
                        status = self.__get_status(json_data["text"])
                    except KeyError as e:
                        _L.error("Unexpected JSON response. Please report this issue!")
                        _L.error(f"JSON data: {json_data}")
                        raise e
                    break

            else:
                _L.debug(f"redirect to '{status.value}'")
                response2 = self.client.get(status.value)
                status = self.__check_redemption_status(response2)

        # workaround for new SHiFT website.
        # it doesn't tell you to launch a "SHiFT-enabled title" anymore
        if status == Status.NONE:
            if redemption:
                status = Status.REDEEMED
            else:
                status = Status.TRYLATER
        return status
