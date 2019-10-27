# AutoSHiFt: Automatically redeem Gearbox SHiFT Codes

- **Platform:** Crossplatform.
- **Version:** 0.5

## Paths
This App saves keys and a login cookie on your local drive.
| MacOs | Linux | Windows |
|-------|-------|---------|
|`~/Library/Application Support/AutoShift`|`~/.local/share/AutoShift`|`C:/Users/<USER>/AppData/Local/AutoShift`|
## Installation
Download and unzip the latest release

### Manual Installation
You will need cmake and Qt5 to build this on your own.

#### Windows

```sh
git clone -branch cpp git@github.com:Fabbi/autoshift.git
cd autoshift
mkdir build
cd build
cmake ..
```

Now you can open the `autoshift.sln` with Visual Studio and compile the code.

#### MacOs/Linux
```sh
git clone -branch cpp git@github.com:Fabbi/autoshift.git
cd autoshift
```

now you can build the release version via `make release` or debug with `make debug`