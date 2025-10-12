# AutoSHiFt: Automatically redeem Gearbox SHiFT Codes

- **Compatibility:** 3.12+.
- **Platform:** Crossplatform.
- **Version:** 2.0.0
- **Repo:** https://github.com/Fabbi/autoshift

# Overview ⚠️ Needs Update ⚠️


Data provided by [ugoogalizer's autoshift-scraper](https://github.com/ugoogalizer/autoshift-scraper).

*This tool doesn't save your login data anywhere on your machine!*
After your first login your login-cookie (a string of seemingly random characters) is saved to the `data` folder and reused every time you use `autoshift` after that.

`autoshift` tries to prevent being blocked when it redeems too many keys at once.

You can choose to only redeem mods/skins etc, only golden keys or both. There is also a limit parameter so you don't waste your keys (there is a limit on how many bl2 keys your account can hold for example).

## Installation

you'll need to install [uv](https://docs.astral.sh/uv/getting-started/installation/#installation-methods)

```sh
curl -LsSf https://astral.sh/uv/install.sh | sh
```

clone repo or download it as zip:

```sh
git clone git@github.com:Fabbi/autoshift.git
```


## Usage

- for help
```sh
uv run autoshift --help
```

- redeem codes for Borderlands 4 on Steam (and keep redeeming every 2 hours)
```sh
uv run autoshift schedule --bl4=steam
```

- redeem codes for Borderlands 4 on Steam using Username and Password (Use quotes for User and Password)
```sh
uv run autoshift schedule --bl4=steam --user "my@user.edu" --pass "p4ssw0rd!123"
```

- redeem a single code
```sh
uv run autoshift redeem steam <code>
```

### Configuration

You can also configure the tool using a `.env` file. See [env.default](env.default) for all possible options.
All those options can also be set via ENV-vars and mixed how you like

Config precedence (from high to low):
- passed command-line arguments
- Environment variables
- `.env`-file entries


# Docker

Available as a docker image based on `python3.12-alpine`

## Usage

All command-line arguments can be used just like running the script directly

```
  docker run --name autoshift \
  --restart=always \
  -v autoshift:/autoshift/data \
  fabianschweinfurth/autoshift:latest schedule --user="<username>" --pass="<password>" --bl4=steam
```

Or with docker-compose:

```
services:
  autoshift:
    container_name: autoshift
    restart: always
    volumes:
      - ./autoshift:/autoshift/data
    image: fabianschweinfurth/autoshift:latest
    command: schedule --user="<username>" --pass="<password>" --bl4=steam
volumes:
  autoshift:
    external: true
    name: autoshift
networks: {}
```
