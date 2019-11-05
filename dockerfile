FROM python:3.8-alpine

ENV SHIFT_GAMES='bl3 blps bl2 bl' \
    SHIFT_PLATFORMS='epic steam' \
    SHIFT_ARGS='--schedule' \
    TZ='America/Chicago'

COPY . /autoshift/
RUN apk update && apk upgrade && \
    apk add --no-cache g++ gcc libxslt-dev && \
    pip install -r ./autoshift/requirements.txt && \
    mkdir ./autoshift/data
CMD python ./autoshift/auto.py --user ${SHIFT_USER} --pass ${SHIFT_PASS} --games ${SHIFT_GAMES} --platforms ${SHIFT_PLATFORMS} ${SHIFT_ARGS}