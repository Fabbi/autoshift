# AutoSHiFt: Automatically redeem Gearbox SHiFT Codes

- **Compatibility:** 3.9+.
- **Platform:** Crossplatform.
- **Version:** 1.1.0
- **Repo:** https://github.com/Fabbi/autoshift

# Overview

Data provided by [Orcicorn's SHiFT and VIP Code archive](https://shift.orcicorn.com/shift-code/).<br>
`autoshift` detects and memorizes new games and platforms added to the orcicorn shift key database.

To see which games and platforms are supported use the `auto.py --help` command.

*This tool doesn't save your login data anywhere on your machine!*
After your first login your login-cookie (a string of seemingly random characters) is saved to the `data` folder and reused every time you use `autoshift` after that.

`autoshift` tries to prevent being blocked when it redeems too many keys at once.

You can choose to only redeem mods/skins etc, only golden keys or both. There is also a limit parameter so you don't waste your keys (there is a limit on how many bl2 keys your account can hold for example).

## Installation

```sh
git clone git@github.com:Fabbi/autoshift.git
```

or download it as zip

you'll need to install a few dependencies

```sh
cd ./autoshift
python3 -m venv venv
source ./venv/bin/activate
pip install -r requirements.txt
mkdir -p ./data
```

## Usage

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

- keep redeeming every 2 hours
```sh
./auto.py --game bl3 --platform steam --schedule
```

- keep redeeming every `n` hours (values < 2 are not possible due to IP Bans)
```sh
./auto.py --game bl3 --platform steam --schedule 5 # redeem every 5 hours
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

## Code

This tool consists of 3 parts:

### `shift.py`

This module handles the redemption of the SHiFT codes and could be used as standalone CLI tool to manually enter those codes.
It queries login credentials on first use and saves the needed cookie to enable auto-login.

### `query.py`

This module parses the codes from wherever they may come from and creates/maintains the database.
If you'd want to add other sources for SHiFT codes or future games, you'd make that here.


### `auto.py`

This one is the commandline interface you call to use this tool.

# Docker

Available as a docker image based on `python3.10-buster`

## Usage

```
docker run \
  --restart=always \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_GAMES='bl3 blps bl2 bl1 ttw' \
  -e SHIFT_PLATFORMS='epic xboxlive psn' \
  -e SHIFT_ARGS='--schedule -v' \
  -e TZ='America/Chicago' \
  -v autoshift:/autoshift/data \
  fabianschweinfurth/autoshift:latest
```

Compose:

```
---
version: "3.0"
services:
  autoshift:
    image: fabianschweinfurth/autoshift:latest
    container_name: autoshift_all
    restart: always
    volumes:
      - autoshift:/autoshift/data
    environment:
      - TZ=America/Denver
      - SHIFT_PLATFORMS=epic xboxlive psn
      - SHIFT_USER=<username>
      - SHIFT_PASS=<password>
      - SHIFT_GAMES=bl3 blps bl2 bl1 ttw gdfll
      - SHIFT_ARGS=--schedule -v
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
|Borderlands|`bl1`|
|Borderlands 2|`bl2`|
|Borderlands: The Pre-Sequel|`blps`|
|Borderlands 3|`bl3`|
|Tiny Tina's Wonderlands|`ttw`|
|Godfall|`gdfll`|


#### **SHIFT_PLATFORM** (recommended)
The platform(s) you want to redeem codes for

Default: `epic steam`

Example: `xbox` or `xbox ps`

|Platform|Code|
|---|---|
|PC (Epic)|`epic`|
|PC (Steam)|`steam`|
|Xbox|`xboxlive`|
|Playstation|`psn`|
|Stadia|`stadia`|


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

## Building Docker Image

``` bash
docker build -t autoshift:latest .

```

## Building Docker Image and Pushing to local Harbor

``` bash

# Once off setup: 
git clone TODO

# Personal parameters
export HARBORURL=harbor.test.com

git pull

#Set Build Parameters
export VERSIONTAG=1.7

#Build the Image
docker build -t autoshift:latest -t autoshift:${VERSIONTAG} . 

#Get the image name, it will be something like 41d81c9c2d99: 
export IMAGE=$(docker images -q autoshift:latest)
echo ${IMAGE}

#Login to local harbor
docker login ${HARBORURL}:443

#Tag and Push the image to local harbor
docker tag ${IMAGE} ${HARBORURL}:443/autoshift/autoshift:latest
docker tag ${IMAGE} ${HARBORURL}:443/autoshift/autoshift:${VERSIONTAG}
docker push ${HARBORURL}:443/autoshift/autoshift:latest
docker push ${HARBORURL}:443/autoshift/autoshift:${VERSIONTAG}

#Tag and Push the image to public docker hub repo
docker login -u ugoogalizer docker.io/ugoogalizer/autoshift
docker tag ${IMAGE} docker.io/ugoogalizer/autoshift:latest
docker tag ${IMAGE} docker.io/ugoogalizer/autoshift:${VERSIONTAG}
docker push docker.io/ugoogalizer/autoshift:latest
docker push docker.io/ugoogalizer/autoshift:${VERSIONTAG}

```