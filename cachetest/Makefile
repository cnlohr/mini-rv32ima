all : cachetest

cachetest : cachetest.c
	gcc -o $@ $^ -O4 -s -I../mini-rv32ima -Wall

clean :
	rm -rf *.o cachetest

