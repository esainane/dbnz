
SUBDIRS = sim asm viz
CLEANDIRS = $(SUBDIRS:%=clean-%)

.PHONY: all clean $(SUBDIRS) $(CLEANDIRS)


all: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean
