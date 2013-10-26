SUBDIR = src

all:
	@for i in $(SUBDIR); do make -C $$i; done

clean:
	@for i in $(SUBDIR); do make -C $$i clean; done

dist-clean: clean
	@for i in $(SUBDIR); do make -C $$i dist-clean; done

