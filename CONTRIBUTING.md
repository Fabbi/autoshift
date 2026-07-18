# Contributing to AutoSHiFt

Thanks for helping improve AutoSHiFt. Bug fixes, documentation updates, tests,
and focused feature contributions are welcome.

## Development setup

AutoSHiFt requires Python 3.9 or newer. Fork and clone the repository, then
create an isolated environment from the repository root:

```sh
python3 -m venv venv
source venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
python -m pip install pre-commit
```

On Windows PowerShell, activate the environment with:

```powershell
venv\Scripts\Activate.ps1
```

Verify the local installation without contacting Gearbox:

```sh
python auto.py --help
```

## Install the pre-commit hooks

Install the repository's Git hooks once after cloning:

```sh
pre-commit install
```

The hooks check for merge markers, validate YAML, fix trailing whitespace and
missing final newlines, and run the Flake8 and mypy versions pinned in
`.pre-commit-config.yaml`. They run automatically when you commit. To check
every tracked file before opening a pull request, run:

```sh
pre-commit run --all-files
```

Some hooks modify files. If that happens, review and stage the changes, then
run the command again. To update hook versions deliberately, use
`pre-commit autoupdate`, review the resulting configuration changes, and rerun
all hooks.

## Project layout

- `auto.py` is the main CLI and coordinates scheduled redemption.
- `query.py` loads SHiFT-code sources and maintains the local database.
- `shift.py` handles Gearbox authentication and code redemption.
- `common.py` contains shared helpers.
- `migrations.py` contains database schema migrations.
- `Dockerfile` and `docker-entrypoint.sh` define container behavior.
- `.github/workflows/` contains repository automation.

Keep changes within the module that already owns the relevant behavior. Put
new automated tests under `tests/`, with files named `test_*.py` and test
functions named `test_*`.

## Making changes safely

Create a branch for each focused change. Follow PEP 8, use four-space
indentation, and use `snake_case` for functions and variables, `PascalCase`
for classes, and uppercase names for constants.

Do not commit credentials, cookies, database files, or populated `data/`
directories. Treat custom `SHIFT_SOURCE` content as untrusted input. Tests
must mock HTTP requests and use temporary databases rather than contacting
Gearbox or modifying a real account. If manual redemption testing is truly
needed, use test credentials and an isolated profile via `AUTOSHIFT_PROFILE`.

## Validate your change

At minimum, run:

```sh
pre-commit run --all-files
python auto.py --help
```

You can also run individual pinned hooks while iterating:

```sh
pre-commit run flake8 --all-files
pre-commit run mypy --all-files
```

If your change affects the container, build it and check the CLI entry point:

```sh
docker build -t autoshift:dev .
docker run --rm -e SHIFT_ARGS=--help autoshift:dev
```

Add focused tests for behavior changes. When a test suite is present, run it
with the test runner used by the repository and include the command and result
in your pull request.

## Commits and pull requests

Use short, imperative commit subjects. Conventional Commit prefixes used by
the project include `fix:`, `docs:`, `build(python):`, and `ci(docker):`.
Keep unrelated changes in separate commits.

Releases follow Semantic Versioning and are automated with Release Please.
Use `fix:` for a patch release, `feat:` for a minor release, and append `!`
or a `BREAKING CHANGE:` footer for a major release. Documentation, CI, and
chore-only commits do not normally trigger a version bump. Release Please
maintains a release pull request; merging it creates the GitHub release and
publishes versioned and `latest` Docker images.

Before submitting a pull request:

1. Rebase or merge the current target branch and resolve any conflicts.
2. Run the validation commands above.
3. Confirm no secrets or runtime files are included.
4. Update documentation for user-visible CLI or environment-variable changes.

In the pull request, provide a concise summary, explain the user-visible
effect, link relevant issues, and list the validation commands and results.
Include logs for runtime fixes. Screenshots are generally only useful for
rendered documentation or workflow UI changes.
