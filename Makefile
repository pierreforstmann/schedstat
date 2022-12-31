all: schedstat 
schedstat: schedstat.c
	$(CC) -std=gnu99 -Wall -o $@ $<
debug: schedstat.c
	$(CC) -g -std=gnu99 -Wall -o $@ $<
clean: 
	rm -f schedstat debug
