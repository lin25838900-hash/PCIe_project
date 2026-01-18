all:
	$(MAKE) -C baseline
	$(MAKE) -C optimized
	$(MAKE) -C tools

clean:
	$(MAKE) -C baseline clean
	$(MAKE) -C optimized clean
	$(MAKE) -C tools clean
