SOURCES=$(wildcard *.c)
HEADERS=$(wildcard *.h)
OBJS := $(SOURCES:.c=.o)

ifeq ($(UNAME), Linux)
TARGET = dbnzc
else
TARGET = dbnzc.exe
endif

BISON=bison
FLEX=flex
CC=gcc
CFLAGS=-g -O2 -fno-strict-aliasing -fstack-protector-all

# Parsing debugging options:
# If we should also generate a file explaining the state explanations
EXPLAIN=-v
# If we should provide a detailed trace whenever we parse a file
RUNTIME_TRACE=-DENABLE_TRACE

all: $(TARGET)

y.tab.c y.tab.h y.output: rdbnz.y rdbnz.h
	$(BISON) -y -d $(EXPLAIN) rdbnz.y

lex.yy.c: rdbnz.l rdbnz.h
	$(FLEX) rdbnz.l

y.tab.o lex.yy.o: y.tab.c lex.yy.c
	$(CC) $(CFLAGS) $(RUNTIME_TRACE) -c y.tab.c lex.yy.c

rdbnz_compile.o: rdbnz_compile.c rdbnz_compile.h

$(TARGET): y.tab.o lex.yy.o rdbnz_compile.o
	$(CC) $(CFLAGS) y.tab.o lex.yy.o rdbnz_compile.o -o $(TARGET)

clean:
	$(RM) y.tab.c y.tab.h y.output lex.yy.c y.tab.o lex.yy.o rdbnz_compile.o $(TARGET)
