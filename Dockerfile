FROM python:3.12-alpine AS base

# copy UV from original distroless image
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /bin/

ENV UV_LINK_MODE=copy
ENV UV_COMPILE_BYTECODE=1
ENV UV_PYTHON_DOWNLOADS=never
# tell UV where to put the virtual env
ENV UV_PROJECT_ENVIRONMENT=/autoshift/.venv
# should be default but better safe then sorry..
ENV UV_CACHE_DIR=/root/.cache
ENV UV_FROZEN=true

WORKDIR /autoshift

RUN --mount=type=cache,dst=/root/.cache/uv \
    --mount=type=bind,src=uv.lock,dst=uv.lock \
    --mount=type=bind,src=pyproject.toml,dst=pyproject.toml \
    uv sync --no-dev

FROM base


ENV VIRTUAL_ENV=/autoshift/.venv
ENV PATH="${VIRTUAL_ENV}/bin:$PATH"
ENV PYTHONPATH=/autoshift
ENV UV_NO_SYNC=true
ENV UV_FROZEN=true

WORKDIR /autoshift

COPY . /autoshift/

ENTRYPOINT ["uv", "run", "--no-dev", "autoshift"]
CMD ["schedule", "--bl4", "steam"]
