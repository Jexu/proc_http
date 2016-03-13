all:
	make -C src
clean:
	make -C src clean
	-rm core
ctags:
	make -C src ctags
