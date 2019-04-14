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

- redeem codes for Borderlands 2 on PC
```sh
./auto.py --game bl2 --platform pc
```

- keep redeeming every hour (you need `apscheduler` for that)
```sh
./auto.py --game bl2 --platform pc --schedule
```

- only redeem golden keys
```sh
./auto.py --game bl2 --platform pc --schedule --golden
```

- only redeem non-golden keys
```sh
./auto.py --game bl2 --platform pc --schedule --non-golden
```

- only redeem up to 30 keys
```sh
./auto.py --game bl2 --platform pc --schedule --golden --limit 30
```

- only query new keys (why though..)
```sh
./auto.py --game bl2 --platform pc --golden --limit 0
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
