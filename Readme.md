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

## Docker Usage

``` bash
docker run \
  --restart=always \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_GAMES='bl4 bl3 blps bl2 bl1 ttw' \
  -e SHIFT_PLATFORMS='epic xboxlive psn nintendo' \
  -e SHIFT_ARGS='--schedule -v' \
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
      - SHIFT_PLATFORMS=epic xboxlive psn
      - SHIFT_USER=<username>
      - SHIFT_PASS=<password>
      - SHIFT_GAMES=bl4 bl3 blps bl2 bl1 ttw gdfll
      - SHIFT_ARGS=--schedule -v
```
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
            - name: SHIFT_PLATFORMS
              value: "epic steam"
            - name: SHIFT_ARGS
              value: "--schedule 6 -v"
            - name: SHIFT_GAMES
              value: "bl4 bl3 blps bl2 bl1 ttw"
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

## Variables

#### **SHIFT_USER** (required)
The username/email for your SHiFT account

Example: `johndoe123`


#### **SHIFT_PASS** (required)
The password for your SHiFT account

Example: `p@ssw0rd`


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