
include make.defs

SUBDIRS = sim asm viz examples
CLEANDIRS = $(SUBDIRS:%=clean-%)

.PHONY: all clean $(SUBDIRS) $(CLEANDIRS)
.NOTPARALLEL:

all: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean
