# AutoSHiFt: Automatically redeem Gearbox SHiFT Codes

- **Compatibility:** Python 2.7+/3.3+.
- **Platform:** Crossplatform. Only tested on MacOs/Linux.
- **Version:** 0.1

WIP

### Installation

```sh
git clone git@github.com:Fabbi/autoshift.git
```

or download it as zip

you'll need to install a few dependencies

```sh
pip install requests beautifulsoup4 lxml
```

or if you want to use the scheduling

```sh
pip install requests beautifulsoup4 lxml apscheduler
```

### Usage

- for help
```sh
./auto.py --help
```

- redeem codes for Borderlands 3 on Steam
```sh
./auto.py --game bl3 --platform steam
```

- redeem codes for Borderlands 3 on Steam using Username and Password (Use quotes for User and Password)
```sh
./auto.py --game bl3 --platform steam --user "my@user.edu" --pass "p4ssw0rd!123"
```

- keep redeeming every hour (you need `apscheduler` for that)
```sh
./auto.py --game bl3 --platform steam --schedule
```

- only redeem golden keys
```sh
./auto.py --game bl3 --platform steam --schedule --golden
```

- only redeem non-golden keys
```sh
./auto.py --game bl3 --platform steam --schedule --non-golden
```

- only redeem up to 30 keys
```sh
./auto.py --game bl3 --platform steam --schedule --golden --limit 30
```

- only query new keys (why though..)
```sh
./auto.py --game bl3 --platform steam --golden --limit 0
```

### Overview

This tool consists of 3 parts:

#### `shift.py`

This module handles the redemption of the SHiFT codes and could be used as standalone CLI tool to manually enter those codes.
It queries login credentials on first use and saves the needed cookie to enable auto-login.

#### `query.py`

This module parses the codes from wherever they may come from ([orcz.com](https://orcz.com) at the moment) and creates/maintains the database.
If you'd want to add other sources for SHiFT codes or future games, you'd make that here.

#### `auto.py`

This one is the commandline interface you call to use this tool.

# Docker
Available as a docker image based on `python3.8-alpine`

## Usage

```
docker run confusingboat/autoshift:latest \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_GAMES='bl3 blps bl2 bl' \
  -e SHIFT_PLATFORMS='epic xbox ps' \
  -e SHIFT_ARGS='--schedule -v' \
  -e TZ='America/Chicago' \
  -v /path/to/keysdb/dir:/autoshift/data
```

## Variables

#### **SHIFT_USER** (required)
The username/email for your SHiFT account

Example: `johndoe123`


#### **SHIFT_PASS** (required)
The password for your SHiFT account

Example: `p@ssw0rd`


#### **SHIFT_GAMES** (recommended)
The game(s) you want to redeem codes for

Default: `bl3 blps bl2 bl`

Example: `blps` or `bl bl2 bl3`

|Game|Code|
|---|---|
|Borderlands|`bl`|
|Borderlands 2|`bl2`|
|Borderlands: The Pre-Sequel|`blps`|
|Borderlands 3|`bl3`|


#### **SHIFT_PLATFORM** (recommended)
The platform(s) you want to redeem codes for

Default: `epic steam`

Example: `xbox` or `xbox ps`

|Platform|Code|
|---|---|
|PC (Epic)|`epic`|
|PC (Steam)|`steam`|
|Xbox|`xbox`|
|Playstation|`ps`|


#### **SHIFT_ARGS** (optional)
Additional arguments to pass to the script

Default: `--schedule`

Example: `--schedule --golden --limit 30`

|Arg|Description|
|---|---|
|`--golden`|Only redeem golden keys|
|`--non-golden`|Only redeem non-golden keys|
|`--limit n`|Max number of golden keys you want to redeem|
|`--schedule`|Keep checking for keys and redeeming every hour|
|`-v`|Verbose mode|


#### **TZ** (optional)
Your timezone

Default: `America/Chicago`

Example: `Europe/London`