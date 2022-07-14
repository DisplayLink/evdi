#
# Copyright (c) 2022 DisplayLink (UK) Ltd.
#
INSTALL ?= install
CP ?= cp
LN ?= ln
RM ?= rm

LIBABI := 0

INCL_DIRS := -I../library $$(python3 -m pybind11 --includes)

CXXFLAGS := $(INCL_DIRS) -std=c++11 -fPIC $(CXXFLAGS)

LDFLAGS := -L../library $(LDFLAGS)

NAME := PyEvdi.so

SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))
LIBS := -levdi

default: $(NAME)

clean:
	rm -rf $(NAME)
	rm -rf $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

$(NAME): $(OBJS)
	$(CXX) $^ $(LDFLAGS) $(LIBS) -Wl,-soname,$(NAME).$(LIBABI) -o $@

PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
tests:
	LD_LIBRARY_PATH=../library python3 -m pytest test/

