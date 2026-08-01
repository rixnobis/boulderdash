TARGET = boulderdash
TYPE = ps-exe

SRCS = \
main.cpp \
tiles.cpp \
cave.cpp \
levels.cpp \
sound.cpp \

CXXFLAGS = -std=c++20

# make DEMO=7 boots straight into cave 7 instead of the title, for screenshots.
# Objects are not flag-tracked, so this forces a rebuild rather than trusting an
# incremental build across a define change - a stale object would silently
# produce the wrong binary and report success.
ifdef DEMO
CXXFLAGS += -DBD_DEMO_LEVEL=$(DEMO)
TARGET = boulderdash-demo
# make DEMO=n DEATH=1 additionally drops a rock on the player, which is the only
# way to photograph the death animation without a controller.
ifdef DEATH
CXXFLAGS += -DBD_DEMO_DEATH=1
endif
endif

PSYQO_ROOT ?= third_party/nugget

include $(PSYQO_ROOT)/psyqo/psyqo.mk
