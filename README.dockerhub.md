# AutoSHiFt Docker Image

Automatically redeem Gearbox SHiFT codes from a container.

This image runs `auto.py` and keeps its state in `/autoshift/data`.

## Quick Start

```sh
docker run \
  --restart=always \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_ARGS='--redeem bl3:steam --schedule -v' \
  -v autoshift:/autoshift/data \
  -e TZ='America/Chicago' \
  zacharmstrong/autoshift:latest
```

## Recommended Configuration

- Mount `/autoshift/data` to persist the cookie and database.
- Pass your credentials with environment variables or secrets.
- Prefer `--redeem` inside `SHIFT_ARGS` for explicit game-to-platform mapping.

## Main Environment Variables

| Variable | Required | Default | Purpose |
| --- | --- | --- | --- |
| `SHIFT_USER` | Yes | none | SHiFT username or email |
| `SHIFT_PASS` | Yes | none | SHiFT password |
| `SHIFT_ARGS` | No | `--schedule` | Extra CLI flags passed to `auto.py` |
| `SHIFT_REDEEM` | No | `bl4:steam ttw:epic bl3:epic bl2:epic,steam blps:epic,steam bl1:steam` | Direct game-to-platform mapping used when set |
| `SHIFT_GAMES` | No | `bl4 bl3 blps bl2 bl1` | Legacy game list fallback |
| `SHIFT_PLATFORMS` | No | `epic steam` | Legacy platform list fallback |
| `SHIFT_SOURCE` | No | bundled default source | Override SHiFT code source URL or file |
| `AUTOSHIFT_PROFILE` | No | none | Use profile-specific state under `/autoshift/data/<profile>/` |
| `TZ` | No | `America/Chicago` | Container timezone |

## Usage Patterns

Use `SHIFT_ARGS` when you want full CLI control:

```sh
docker run \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_ARGS='--redeem bl3:steam,epic bl2:epic --schedule --golden -v' \
  -v autoshift:/autoshift/data \
  zacharmstrong/autoshift:latest
```

Use `SHIFT_REDEEM` when you want the container defaults plus an explicit mapping:

```sh
docker run \
  -e SHIFT_USER='<username>' \
  -e SHIFT_PASS='<password>' \
  -e SHIFT_REDEEM='bl3:steam,epic bl2:epic' \
  -e SHIFT_ARGS='--schedule -v' \
  -v autoshift:/autoshift/data \
  zacharmstrong/autoshift:latest
```

## Docker Compose

```yaml
services:
  autoshift:
    image: zacharmstrong/autoshift:latest
    container_name: autoshift
    restart: always
    environment:
      TZ: America/Chicago
      SHIFT_USER: <username>
      SHIFT_PASS: <password>
      SHIFT_ARGS: --redeem bl3:steam --schedule -v
    volumes:
      - autoshift:/autoshift/data

volumes:
  autoshift:
```

## Notes

- The container stores cookies and the redemption database in `/autoshift/data`.
- If `SHIFT_REDEEM` is set, it takes precedence over the legacy `SHIFT_GAMES` and `SHIFT_PLATFORMS` flow.
- Avoid committing credentials in compose files; prefer Docker secrets or environment injection from your runtime.

Full project documentation: <https://github.com/zarmstrong/autoshift>
