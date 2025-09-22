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
import logging
from enum import StrEnum, auto
from pathlib import Path
from typing import Annotated, Any, Literal, override

import rich.logging
from pydantic import BeforeValidator, Field, SecretStr
from pydantic_settings import BaseSettings, SettingsConfigDict

ROOT_DIR = Path(__file__).parent.parent


class Game(StrEnum):
    bl1 = auto()
    bl2 = auto()
    bl3 = auto()
    bl4 = auto()
    blps = auto()
    ttw = auto()
    gdfll = auto()
    UNKNOWN = auto()


class Platform(StrEnum):
    steam = auto()
    epic = auto()
    psn = auto()
    xboxlive = auto()
    stadia = auto()


def path(v: str):
    return Path(v)


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=(ROOT_DIR / ".env"),
        env_prefix="SHIFT_",
        env_file_encoding="utf-8",
        extra="ignore",
    )
    COOKIE_FILE: Annotated[Path, BeforeValidator(path)] = (
        ROOT_DIR / "data" / ".cookies.save"
    )
    DB_FILE: Annotated[Path, BeforeValidator(path)] = ROOT_DIR / "data" / "keys.db"

    LOG_LEVEL: Literal["DEBUG", "INFO", "WARNING", "ERROR"] = "WARNING"

    PLATFORMS: list[Platform] = Field(default_factory=list)
    GAMES: list[Game] = Field(default_factory=list)

    USER: str | None = Field(default=None)
    PASS: SecretStr | None = Field(default=None)

    SCHEDULE: int | None = Field(default=120)

    LIMIT: int = Field(default=255, gt=1)

    @override
    def model_post_init(self, context: Any, /) -> None:
        super().model_post_init(context)
        self.COOKIE_FILE.parent.mkdir(parents=True, exist_ok=True)
        self.DB_FILE.parent.mkdir(parents=True, exist_ok=True)


# settings

settings = Settings()

# setup logging
_L = logging.getLogger("autoshift")

handler = rich.logging.RichHandler(level=settings.LOG_LEVEL, show_path=False)
_L.addHandler(handler)
