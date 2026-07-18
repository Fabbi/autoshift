# Repository Guidelines

## Project Structure & Module Organization

AutoSHiFt is a Python CLI with a deliberately flat module layout. `auto.py` is the main entry point and coordinates scheduled redemption. `query.py` retrieves SHiFT-code data and maintains the local database, while `shift.py` handles Gearbox authentication and code redemption. Shared helpers live in `common.py`, and schema updates belong in `migrations.py`. Container behavior is defined by `Dockerfile` and `docker-entrypoint.sh`; publishing automation lives in `.github/workflows/`. Release Please is configured by `release-please-config.json`, `.release-please-manifest.json`, and `.github/workflows/release-please.yml`. Runtime state, including cookies and `keys.db`, is stored under `data/` and must not be committed.

## Build, Test, and Development Commands

Use Python 3.9 or newer. Create an isolated environment and install dependencies:

```sh
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

Run `python auto.py --help` to verify the CLI or `python auto.py --redeem bl3:steam` for a real invocation. Because redemption contacts external services and changes account state, use test credentials and a separate profile when experimenting. Build the container with `docker build -t autoshift:dev .`; check it with `docker run --rm -e SHIFT_ARGS=--help autoshift:dev`.

## Coding Style & Naming Conventions

Follow PEP 8 conventions: four-space indentation, `snake_case` for functions and variables, `PascalCase` for classes, and uppercase names for constants. Keep modules focused on their current responsibilities. Development tooling declared in `pyproject.toml` includes Flake8, Pylint, mypy, and autoflake. Before submitting, run:

```sh
flake8 *.py
mypy *.py
```

## Testing Guidelines

There is currently no committed automated test suite or coverage threshold. For behavior changes, add focused tests under `tests/` using names such as `test_query.py` and functions such as `test_loads_local_shift_source`. Avoid live Gearbox requests in tests; mock HTTP calls and use temporary databases. At minimum, exercise `python auto.py --help` and the affected code path locally.

## Commit & Pull Request Guidelines

Use short, imperative Conventional Commit subjects and keep each commit scoped. Release Please derives Semantic Versioning increments from these subjects: `fix:` produces a patch release, `feat:` produces a minor release, and a `!` suffix or `BREAKING CHANGE:` footer produces a major release. Prefixes such as `docs:`, `ci:`, and `chore:` do not normally trigger a release. Use scopes such as `ci(docker):` and `build(python):` where helpful.

Release Please owns version updates through its release pull request. Avoid manually editing the version in `pyproject.toml`, `.release-please-manifest.json`, release tags, or generated changelog entries except when bootstrapping Release Please for the first time.

When writing or suggesting a commit message, inspect and describe only staged changes (`git diff --cached`); do not include unstaged or untracked work. Choose the Conventional Commit type according to the staged change and its intended SemVer impact rather than merely the files touched. Pull requests should include a concise summary, validation commands and results, linked issues, and documentation updates for CLI or environment-variable changes. Include logs for runtime fixes; screenshots are only useful for rendered documentation or workflow UI changes. Merging the Release Please pull request creates the `vX.Y.Z` GitHub release and publishes `X.Y.Z`, `latest`, and commit-SHA Docker tags.

## Security & Configuration

Never commit credentials, cookies, databases, or populated `data/` directories. Supply secrets through environment variables such as `SHIFT_USER` and `SHIFT_PASS`, and use `AUTOSHIFT_PROFILE` to isolate development state. Treat custom `SHIFT_SOURCE` content as untrusted input.
