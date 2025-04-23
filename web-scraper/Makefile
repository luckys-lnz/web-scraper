CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -g -D_FORTIFY_SOURCE=2 -fstack-protector-strong -D_FILE_OFFSET_BITS=64 -pthread -I/usr/include/libxml2
LDFLAGS = -pthread -lhiredis -lcurl -lm -lxml2
CFLAGS += $(shell pkg-config --cflags libxml-2.0)
LDFLAGS += $(shell pkg-config --libs libxml-2.0)

# Dependencies
LIBS = -lcurl -lxml2 -lhiredis -lpthread -lm

# Source files
SRCS = main.c scraper.c fetch_url.c redis_helper.c robots_parser.c robots_rules.c thread_pool.c \
       logger.c cache.c rate_limiter.c extract_title.c extract_meta.c \
       extract_hrefs.c write_callback.c stats.c url_processor.c content_analyzer.c
OBJS = $(SRCS:.c=.o)

# Header files
HEADERS = scraper.h fetch_url.h redis_helper.h robots_parser.h robots_rules.h thread_pool.h \
          logger.h cache.h rate_limiter.h write_callback.h stats.h url_processor.h

# Targets
TARGET = webscraper

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

debug: CFLAGS += -DDEBUG -g3
debug: clean all

prod: CFLAGS += -O3 -DNDEBUG
prod: clean all

test: $(TARGET)
	./$(TARGET)

analyze: CFLAGS += -fanalyzer
analyze: clean all

format:
	clang-format -i $(SRCS) $(HEADERS)

install-deps:
	sudo apt-get install -y libcurl4-openssl-dev libxml2-dev libhiredis-dev

.PHONY: all clean debug prod test analyze format install-deps
