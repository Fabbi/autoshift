# AutoSHiFt: Automatically redeem Gearbox SHiFT Codes

- **Compatibility:** 3.9+.
- **Platform:** Crossplatform.
- **Repo:** https://github.com/zarmstrong/autoshift forked from https://github.com/ugoogalizer/autoshift forked from https://github.com/Fabbi/autoshift

# Overview

Data provided by Mental Mars' Websites via this [shiftcodes.json](https://raw.githubusercontent.com/zarmstrong/autoshift-codes/main/shiftcodes.json) file that is updated reguarly by an instance of [this scraper](https://github.com/zarmstrong/autoshift-scraper).  You don't need to run the scraper as well, only this `autoshift` script/container.  This is to reduce the burden on the Mental Mars website given the great work they're doing to make this possible.<br>

This documentation intended to be temporary until Issue [#53](https://github.com/Fabbi/autoshift/issues/53) and PR [#54](https://github.com/Fabbi/autoshift/pull/54) in the the upstream [autoshift by Fabbi](https://github.com/Fabbi/autoshift/) is merged in.

`autoshift` detects and memorizes new games and platforms added to the orcicorn shift key database.

Games currently maintained by mental mars that are scraped and made available to `autoshift` are: 
- [Borderlands](https://mentalmars.com/game-news/borderlands-golden-keys/)
- [Borderlands 2](https://mentalmars.com/game-news/borderlands-2-golden-keys/)
- [Borderlands 3](https://mentalmars.com/game-news/borderlands-3-golden-keys/)
- [Borderlands 4](https://mentalmars.com/game-news/borderlands-4-shift-codes/)
- [Borderlands The Pre-Sequel](https://mentalmars.com/game-news/bltps-golden-keys/)
- [Tiny Tina's Wonderlands](https://mentalmars.com/game-news/tiny-tinas-wonderlands-shift-codes)

To see which games and platforms are supported use the `auto.py --help` command.

*This tool doesn't save your login data anywhere on your machine!*
After your first login your login-cookie (a string of seemingly random characters) is saved to the `data` folder and reused every time you use `autoshift` after that.

`autoshift` tries to prevent being blocked when it redeems too many keys at once.

You can choose to only redeem mods/skins etc, only golden keys or both. There is also a limit parameter so you don't waste your keys (there is a limit on how many bl2 keys your account can hold for example).

## Installation

```sh
git clone git@github.com:zarmstrong/autoshift.git
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

# ⚠️ Database Change Notice

**As of the 9/18/2025 version, the way redeemed keys are tracked has changed.**  
Redemption status is now tracked in a separate table for each key and platform combination.  

**On first run after upgrade, all keys will be retried to ensure the database is properly marked. This may take a long time if you have a large key database.**  
This is expected and only happens once; subsequent runs will be fast.

## Usage Instructions

You can now specify exactly which platforms should redeem which games' SHiFT codes using the `--redeem` argument.  
**Recommended:** Use `--redeem` for fine-grained control.  
**Legacy:** You can still use `--games` and `--platforms` together, but a warning will be printed and all games will be redeemed on all platforms.

### New (Recommended) Usage

- Redeem codes for Borderlands 3 on Steam and Epic, and Borderlands 2 on Epic only:
```sh
./auto.py --redeem bl3:steam,epic bl2:epic
```

- Redeem codes for Borderlands 3 on Steam only:
```sh
./auto.py --redeem bl3:steam
```

- You can still use other options, e.g.:
```sh
./auto.py --redeem bl3:steam,epic --golden --limit 10
```

### Legacy Usage (still supported, but prints a warning)

- Redeem codes for Borderlands 3 and Borderlands 2 on Steam and Epic (all combinations):
```sh
./auto.py --games bl3 bl2 --platforms steam epic
```

## Override SHiFT source

You can override the default SHiFT codes source (the JSON that is normally fetched from mentalmars/autoshift-codes) with either a URL or a local file path.

- CLI: use the --shift-source argument. Example:
```sh
./auto.py --shift-source "https://example.com/my-shift-codes.json" --redeem bl3:steam
```

- Environment variable: set SHIFT_SOURCE. The CLI flag takes precedence over the environment variable.
```sh
export SHIFT_SOURCE="file:///path/to/shiftcodes.json"
# or
export SHIFT_SOURCE="/absolute/path/to/shiftcodes.json"
```

Supported formats:
- HTTP(s) URLs (e.g. https://...)
- Local absolute or relative paths (e.g. ./data/shiftcodes.json)
- file:// URLs (e.g. file:///autoshift/data/shiftcodes.json)

Docker / Kubernetes examples
- Docker run (override source via env):
```sh
docker run \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_SOURCE='https://example.com/shiftcodes.json' \
  -e SHIFT_ARGS='--redeem bl3:steam --schedule -v' \
  -v autoshift:/autoshift/data \
  zacharmstrong/autoshift:latest
```

- Kubernetes (set SHIFT_SOURCE in the manifest):
```yaml
env:
  - name: SHIFT_SOURCE
    value: "https://example.com/shiftcodes.json"
  - name: SHIFT_ARGS
    value: "--redeem bl3:steam --schedule 6 -v"
```

Notes
- CLI --shift-source overrides the SHIFT_SOURCE environment variable.
- If the source is a local path inside the container, ensure the file is present in the container filesystem (mounted volume, image, etc.).
- The tool validates and logs the selected source at startup so you can confirm which file/URL is being used.

## Profiles (per-user data)

You can run autoshift with named profiles so separate runs (or different users) keep their own DB, cookies and other state.

- Default: when no profile is specified autoshift uses the normal data directory:
  c:\Users\slate\Dropbox\code\autoshift\data

- Profile mode: when a profile is specified, autoshift will use a profile-specific data path:
  c:\Users\slate\Dropbox\code\autoshift\data/<profile>/
  That directory holds the profile's keys.db, cookie file and any other state.

How to select a profile:
- CLI (takes precedence):
  ./auto.py --profile myprofile --redeem bl3:steam
- Environment variable:
  export AUTOSHIFT_PROFILE=myprofile
  ./auto.py --redeem bl3:steam

Notes:
- The CLI --profile overrides AUTOSHIFT_PROFILE for that run.
- Each profile has its own DB and cookie file; migrations and initial key processing run per-profile.
- First run for a profile may take longer because keys are re-processed and the DB is initialized/migrated for that profile.
- Use profiles when you want isolated state (e.g. different accounts, test vs production, or per-container environments).

Docker / Kubernetes examples
- Docker (profile via env):
```sh
docker run \
  --restart=always \
  -e AUTOSHIFT_PROFILE='myprofile' \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_ARGS='--redeem bl3:steam --schedule -v' \
  -v autoshift:/autoshift/data \
  zacharmstrong/autoshift:latest
```

- Docker (profile via SHIFT_ARGS):
```sh
docker run \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_ARGS='--profile myprofile --redeem bl3:steam --schedule -v' \
  -v autoshift:/autoshift/data \
  zacharmstrong/autoshift:latest
```

- Kubernetes (set AUTOSHIFT_PROFILE in the manifest):
```yaml
env:
  - name: AUTOSHIFT_PROFILE
    value: "myprofile"
  - name: SHIFT_ARGS
    value: "--redeem bl3:steam --schedule 6 -v"
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

## Docker Usage

``` bash
docker run \
  --restart=always \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_ARGS='--redeem bl3:steam,epic bl2:epic --schedule -v' \
  -e TZ='America/Chicago' \
  -v autoshift:/autoshift/data \
  zacharmstrong/autoshift:latest
```

## Docker Compose Usage:

``` yaml
---
version: "3.0"
services:
  autoshift:
    image: zacharmstrong/autoshift:latest
    container_name: autoshift_all
    restart: always
    volumes:
      - autoshift:/autoshift/data
    environment:
      - TZ=America/Denver
      - SHIFT_USER=<username>
      - SHIFT_PASS=<password>
      - SHIFT_ARGS=--redeem bl3:steam,epic bl2:epic --schedule -v
```

> **Note:**  
> When using Docker, set the `SHIFT_ARGS` environment variable to include your `--redeem ...` options.  
> If you use both `SHIFT_GAMES`/`SHIFT_PLATFORMS` and `--redeem`, the `--redeem` mapping will take precedence and a warning will be printed if legacy options are also present.

## Kubernetes Usage:

After setting up the secrets in K8s first: 
```bash
kubectl create namespace autoshift
kubectl config set-context --current --namespace=autoshift
kubectl create secret generic autoshift-secret --from-literal=username='XXX' --from-literal=password='XXX'

# To get/check the username and password use: 
kubectl get secret autoshift-secret -o jsonpath="{.data.username}" | base64 -d
kubectl get secret autoshift-secret -o jsonpath="{.data.password}" | base64 -d
```
Then deploy with something similar to: 
``` yaml
--- # deployment
apiVersion: apps/v1
kind: Deployment
metadata:
  labels:
    app: autoshift
  name: autoshift
#  namespace: autoshift
spec:
  selector:
    matchLabels:
      app: autoshift
  revisionHistoryLimit: 0
  template:
    metadata:
      labels:
        app: autoshift
    spec:
      containers:
        - name: autoshift
          image: zacharmstrong/autoshift:latest
          imagePullPolicy: IfNotPresent
          env:
            - name: SHIFT_USER
              valueFrom:
                secretKeyRef:
                  name: autoshift-secret
                  key: username
            - name: SHIFT_PASS
              valueFrom:
                secretKeyRef:
                  name: autoshift-secret
                  key: password
            - name: SHIFT_ARGS
              value: "--redeem bl3:steam,epic bl2:epic --schedule 6 -v"
            - name: TZ
              value: "Australia/Sydney"
          resources:
            requests:
              cpu: 100m
              memory: 500Mi
            limits:
              cpu: "100m"
              memory: "500Mi"
          volumeMounts:
            - mountPath: /autoshift/data
              name: autoshift-pv
      volumes:
        - name: autoshift-pv
          # If this is NFS backed, you may have to add the nolock mount option to the storage class
          persistentVolumeClaim:
            claimName: autoshift-pvc
---
apiVersion: v1
kind: PersistentVolumeClaim
# If this is NFS backed, you may have to add the nolock mount option to the storage class
metadata:
  name: autoshift-pvc
#  namespace: autoshift
spec:
  storageClassName: managed-nfs-storage-retain
  accessModes:
    - ReadWriteMany
  resources:
    requests:
      storage: 1Gi

```

> **Note:**  
> When using Kubernetes, set the `SHIFT_ARGS` environment variable in your deployment manifest to include your `--redeem ...` options.  
> If you use both `SHIFT_GAMES`/`SHIFT_PLATFORMS` and `--redeem`, the `--redeem` mapping will take precedence and a warning will be printed if legacy options are also present.

## Variables

#### **SHIFT_USER** (required)
The username/email for your SHiFT account

Example: `johndoe123`


#### **SHIFT_PASS** (required)
The password for your SHiFT account

Example: `p@ssw0rd`

Important: shells (especially interactive bash) may perform history expansion on the exclamation mark `!`. If you pass a password directly on the command line and it contains `!` the shell may replace/truncate it before autoshift sees it.

Troubleshooting / recommended ways to provide the password:
- Preferred (safe): set the password via environment variable (avoids shell history issues).
  - Bash:
    ```sh
    export SHIFT_PASS='p@ss!word'    # use single quotes to prevent history expansion
    ./auto.py --user you --pass "${SHIFT_PASS}" --redeem bl3:steam
    ```
  - Docker:
    ```sh
    docker run -e SHIFT_PASS='p@ss!word' ...
    ```
  - Kubernetes: store the password in a Secret and set SHIFT_PASS from the secret (recommended).
- If you must pass the password on the CLI, wrap it in single quotes to avoid history expansion:
  ```sh
  ./auto.py -u you -p 'p@ss!word' --redeem bl3:steam
  ```
- Alternative: escape the `!` character (less recommended than quoting).
- Debugging: to temporarily print the password in logs for debugging, set:
  ```sh
  export AUTOSHIFT_DEBUG_SHOW_PW=1
  ```
  Security warning: this will print your password in cleartext to the logs — use only for short-lived debugging and remove the env var afterwards.

Notes about autoshift behavior:
- The tool contains a heuristic: if the CLI-provided password contains `!` and an environment password (SHIFT_PASS or AUTOSHIFT_PASS_RAW) appears longer, the environment value will be preferred. Still, supplying the full password via SHIFT_PASS (or via a container secret) is the most reliable method.
- Do not commit passwords to command history or scripts. Use environment variables, container secrets, or mounted files / K8s Secrets.

#### **SHIFT_GAMES** (recommended)
The game(s) you want to redeem codes for

Default: `bl4 bl3 blps bl2 bl`

Example: `blps` or `bl bl2 bl3`

|Game|Code|
|---|---|
|Borderlands|`bl1`|
|Borderlands 2|`bl2`|
|Borderlands: The Pre-Sequel|`blps`|
|Borderlands 3|`bl3`|
|Borderlands 4|`bl4`|
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
|Nintendo|`nintendo`|


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

## Building Docker Image and Pushing to local Harbor and/or Docker Hub

``` bash

# Once off setup: 
git clone TODO

# Personal parameters
export HARBORURL=harbor.test.com

git pull

#Set Build Parameters
export VERSIONTAG=1.8

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
docker login -u zacharmstrong docker.io/zacharmstrong/autoshift
docker tag ${IMAGE} docker.io/zacharmstrong/autoshift:latest
docker tag ${IMAGE} docker.io/zacharmstrong/autoshift:${VERSIONTAG}
docker push docker.io/zacharmstrong/autoshift:latest
docker push docker.io/zacharmstrong/autoshift:${VERSIONTAG}

```