SOURCES=$(wildcard *.c)
HEADERS=$(wildcard *.h)
DEPS=$(wildcard *.d)
OBJS := $(SOURCES:.c=.o)

ifeq ($(UNAME), Linux)
SOSUFFIX = so
LIBPREFIX = lib
else
SOSUFFIX = dll
LIBPREFIX =
endif

LIB_NAME=dbnz
LIB_TARGET=$(LIBPREFIX)$(LIB_NAME).$(SOSUFFIX)

CC=gcc
CFLAGS=-g -O2 -fno-strict-aliasing

$(LIB_TARGET): $(OBJS) $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(OBJS) -Wl,-soname,$(LIB_TARGET) -shared -o $(LIB_TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(LIBFLAGS) -MMD -MP -MT $@ -c $<

# Presumption - if a .o is already built, there is a suitably up to date .d file with it.
# Still may need to run make twice in some degenerate cases where you move headers around
-include $(OBJS:.o=.d)

clean:
	$(RM) $(OBJS) $(DEPS) $(LIB_TARGET)
