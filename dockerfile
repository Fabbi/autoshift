FROM python:3.10-slim

ENV SHIFT_GAMES='bl3 blps bl2 bl1' \
    SHIFT_PLATFORMS='epic steam' \
    SHIFT_ARGS='--schedule' \
    TZ='America/Chicago'

COPY . /autoshift/
RUN pip install -r ./autoshift/requirements.txt && \
    mkdir -p ./autoshift/data
CMD python ./autoshift/auto.py --user ${SHIFT_USER} --pass ${SHIFT_PASS} --games ${SHIFT_GAMES} --platforms ${SHIFT_PLATFORMS} ${SHIFT_ARGS}