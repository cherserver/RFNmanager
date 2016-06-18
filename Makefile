VERSION = 0.0.1
MGR = rfnmgr
LANGS = en

LIB += rfnmgr
rfnmgr_SOURCES = tables.cpp db_user.cpp db_auth.cpp
rfnmgr_LDADD = -lbase

include ../isp.mk
