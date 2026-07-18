FROM python:3.14-alpine

ENV SHIFT_GAMES='bl4 bl3 blps bl2 bl1' \
    SHIFT_PLATFORMS='epic steam' \
    SHIFT_REDEEM='bl4:steam ttw:epic bl3:epic bl2:epic,steam blps:epic,steam bl1:steam' \
    SHIFT_ARGS='--schedule' \
    TZ='America/Chicago'

COPY . /autoshift/
RUN pip install -r ./autoshift/requirements.txt && \
    mkdir -p ./autoshift/data
RUN chmod +x ./autoshift/docker-entrypoint.sh
ENTRYPOINT ["./autoshift/docker-entrypoint.sh"]
