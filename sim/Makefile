SOURCES=$(wildcard *.c)
HEADERS=$(wildcard *.h)
OBJS := $(SOURCES:.c=.o)
LIB_NAME=dbnz
LIB_TARGET=lib$(LIB_NAME).so

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
	rm -f *.o  *.d $(LIB_TARGET)
