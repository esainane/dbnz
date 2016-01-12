
include ../make.defs

SOURCES:=main.c $(wildcard rdbnz_*.c)
HEADERS:=$(wildcard rdbnz_*.h)
DEPS:=$(wildcard *.d)
OBJS:=$(SOURCES:.c=.o)

ifeq ($(UNAME), Linux)
TARGET=dbnzc
else
TARGET=dbnzc.exe
endif

BISON?=bison
FLEX?=flex
CC?=gcc
CFLAGS:=$(CFLAGS) -g -O2 -fstack-protector-all
override CFLAGS+= -fno-strict-aliasing
BISONFLAGS:=$(BISONFLAGS) -y -d

# There isn't much we can do about generated sources. It might be useful to periodically override this to be sure nothing is caused by something we added, but too much warning blindness inducing noise for the default.
NOEXTWARN?=-Wno-sign-compare -Wno-unused-function -Wno-unneeded-internal-declaration -Wno-unused-parameter

ifeq ($(VERBOSE), 1)
# Parsing debugging options:
# If we should also generate a file explaining the state explanations
BISONFLAGS:=$(BISONFLAGS) -v
# If we should provide a detailed trace whenever we parse a file
override CFLAGS += -DENABLE_TRACE
endif

all: $(TARGET)

y.tab.c y.tab.h y.output: rdbnz.y rdbnz.h
	$(BISON) $(BISONFLAGS) rdbnz.y

lex.yy.c lex.yy.h: rdbnz.l rdbnz.h
	$(FLEX) --header-file=lex.yy.h rdbnz.l

y.tab.o lex.yy.o: y.tab.c lex.yy.c
	$(CC) $(CFLAGS) $(NOEXTWARN) -c y.tab.c lex.yy.c

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -MT $@ -c $<

# Presumption - if a .o is already built, there is a suitably up to date .d file with it.
# Still may need to run make twice in some degenerate cases where you move headers around
-include $(OBJS:.o=.d)

$(TARGET): y.tab.o lex.yy.o $(OBJS)
	$(CC) $(CFLAGS) y.tab.o lex.yy.o $(OBJS) -o $(TARGET)

clean:
	$(RM) y.tab.c y.tab.h y.output lex.yy.c y.tab.o lex.yy.o $(DEPS) $(OBJS) $(TARGET)
