MODULE_big = pg_hspell
OBJS = dict_hspell.o

SHLIB_LINK = -lhspell

EXTENSION = pg_hspell
DATA = pg_hspell--1.0.sql
DATA_TSEARCH = hebrew.stop

REGRESS = dict_hspell

PG_CONFIG = pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
