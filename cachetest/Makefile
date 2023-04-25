all : cachetest

cachetest : cachetest.c
	gcc -o $@ $^ -Os -I../mini-rv32ima

clean :
	rm -rf *.o cachetest

