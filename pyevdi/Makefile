#
# Copyright (c) 2022 DisplayLink (UK) Ltd.
#

INSTALL ?= install
CP ?= cp
LN ?= ln
RM ?= rm

LIBABI := 1

INCL_DIRS := -I../library $$(python3 -m pybind11 --includes)

# -fvisibility=hidden https://pybind11.readthedocs.io/en/stable/faq.html#someclass-declared-with-greater-visibility-than-the-type-of-its-field-someclass-member-wattributes
CXXFLAGS := $(INCL_DIRS) -std=c++20 -fPIC -fvisibility=hidden $(CXXFLAGS)

LDFLAGS := -L../library -Wl,--no-undefined $$(python3-config --ldflags --embed) -shared

NAME_SUFFIX := $$(python3-config --extension-suffix)
NAME := PyEvdi$(NAME_SUFFIX)

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
	$(CXX) $^ $(LDFLAGS) $(LIBS) -Wl,-soname,$(NAME).so.$(LIBABI) -o $(NAME)


tests:
	LD_LIBRARY_PATH=../library python3 -m pytest test/


LIBDIR ?= $$(python3 -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())")


install:
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 755 $(NAME) $(DESTDIR)$(LIBDIR)/$(NAME)
	$(LN) -sf $(NAME) $(DESTDIR)$(LIBDIR)/$(NAME).$(LIBABI)

uninstall:
	$(RM) -f $(DESTDIR)$(LIBDIR)/$(NAME)
	$(RM) -f $(DESTDIR)$(LIBDIR)/$(NAME).$(LIBABI)
