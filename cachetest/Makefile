all : cachetest

cachetest : cachetest.c
	gcc -o $@ $^ -Og -g -I../mini-rv32ima

clean :
	rm -rf *.o cachetest

