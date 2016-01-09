SOURCES=$(wildcard *.c)
HEADERS=$(wildcard *.h)
OBJS := $(SOURCES:.c=.o)
TARGET=dbnzc

BISON=bison
FLEX=flex
CC=gcc
CFLAGS=-g -O2 -fno-strict-aliasing -fstack-protector-all

all: $(TARGET) explain

explain: rdbnz.output

y.tab.c y.tab.h: rdbnz.y rdbnz.h
	$(BISON) -y -d rdbnz.y

rdbnz.output: rdbnz.y
	$(BISON) -y -v rdbnz.y

lex.yy.c: rdbnz.l rdbnz.h
	$(FLEX) rdbnz.l

y.tab.o lex.yy.o: y.tab.c lex.yy.c
	$(CC) $(CFLAGS) -c y.tab.c lex.yy.c

rdbnz_compile.o: rdbnz_compile.c rdbnz_compile.h

$(TARGET): y.tab.o lex.yy.o rdbnz_compile.o
	$(CC) $(CFLAGS) y.tab.o lex.yy.o rdbnz_compile.o -o $(TARGET)

clean:
	rm y.tab.c y.tab.h rdbnz.output lex.yy.c y.tab.o lex.yy.o rdbnz_compile.o $(TARGET)
