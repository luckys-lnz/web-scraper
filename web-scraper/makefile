CC = gcc
CFLAGS = -Wall -pthread $(shell pkg-config --cflags hiredis libcurl libxml-2.0)
LDFLAGS = $(shell pkg-config --libs hiredis libcurl libxml-2.0)

OBJS = scraper.o redis_helper.o fetch_url.o extract_hrefs.o extract_meta.o \
       extract_title.o write_callback.o robots_parser.o

all: webcrawler

webcrawler: $(OBJS)
	$(CC) -o webcrawler $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f webcrawler $(OBJS)

