import logging
from logging import NOTSET, DEBUG, INFO, WARNING, ERROR, CRITICAL

GLOBAL_LVL = INFO
# GLOBAL_LVL = DEBUG

colors = {
    NOTSET: 36,
    DEBUG: 33,
    INFO: 34,
    WARNING: .35,
    ERROR: 31,
    CRITICAL: "5;31"
}


def getLogger(name):
    logger = logging.getLogger(name)

    def rec_filter(record):
        record.module_lineno = ""
        if record.levelno == DEBUG:
            record.module_lineno = "\033[1;36m{}:{} - ".format(record.module,
                                                               record.lineno)
        record.color = colors[record.levelno]
        record.spaces = " " * (8 - len(record.levelname))
        return True

    h = logging.StreamHandler()
    h.setFormatter(
        logging.Formatter("\r{bcolor}%(asctime)s "
                          "[\033[1;%(color)sm%(levelname)s\033[0m{bcolor}] "
                          "\033[0m%(spaces)s"
                          "%(module_lineno)s"


                          "\033[0m%(message)s"
                          .format(bcolor="\033[1;36m")))
    h.addFilter(rec_filter)
    logger.handlers = []
    logger.addHandler(h)
    return logger
