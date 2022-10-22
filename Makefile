all: schedstat 
schedstat: schedstat.c
	$(CC) -std=gnu99 -Wall -o $@ $<
clean: 
	rm -f schedstat 
