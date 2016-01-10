
OBJS=$(wildcard *.dbnz)

ifeq ($(UNAME), Linux)
CC = ../asm/dbnzc
else
CC = ../asm/dbnzc.exe
endif
MEMORY = 65536

helloworld.dbnz: helloworld.rdbnz stdlib.rdbnz $(CC)
	$(CC) $(MEMORY) $< $@

clean:
	$(RM) $(OBJS)
