VERSION = 0.0.1
MGR = rfnmgr
LANGS = en

LIB += rfnmgr
rfnmgr_SOURCES = tables.cpp db_user.cpp db_auth.cpp radio.cpp stats.cpp
rfnmgr_LDADD += -lbase -lostemplate
rfnmgr_DLIBS += rf24-bcm rf24network

include ../isp.mk
