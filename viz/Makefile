
include ../make.defs

SOURCES=$(wildcard *.c)
HEADERS=$(wildcard *.h)
DEPS=$(wildcard *.d)
OBJS:=$(SOURCES:.c=.o)

ifeq ($(UNAME), Linux)
TARGET=inspect
else
TARGET=inspect.exe
endif

CC?=gcc
CFLAGS:=$(CFLAGS) -g -O2
override CFLAGS += -fno-strict-aliasing -I../sim
LFLAGS:=$(LFLAGS) -L../sim -ldbnz

all: $(TARGET)
$(TARGET): $(OBJS) $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -MT $@ -c $<

# Presumption - if a .o is already built, there is a suitably up to date .d file with it.
# Still may need to run make twice in some degenerate cases where you move headers around
-include $(OBJS:.o=.d)

clean:
	$(RM) $(DEPS) $(OBJS) $(TARGET)
