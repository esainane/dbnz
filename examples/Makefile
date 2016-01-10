
include ../make.defs

OBJS:=$(wildcard *.dbnz)

ifeq ($(UNAME), Linux)
DBNZC?=../asm/dbnzc
DBNZRE?=../viz/inspect
else
DBNZC?=../asm/dbnzc.exe
DBNZRE?=../viz/inspect.exe
endif
MEMORY=65536

TARGET?=helloworld.dbnz

%.dbnz: %.rdbnz stdlib.rdbnz $(DBNZC)
	$(DBNZC) $(MEMORY) $< $@

clean:
	$(RM) $(OBJS)

run: $(TARGET) $(DBNZRE)
	LD_LIBRARY_PATH=../sim/ $(DBNZRE) $(TARGET)
