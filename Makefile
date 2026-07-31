TARGET = boulderdash
TYPE = ps-exe

SRCS = \
main.cpp \
tiles.cpp \

CXXFLAGS = -std=c++20

PSYQO_ROOT ?= third_party/nugget

include $(PSYQO_ROOT)/psyqo/psyqo.mk
