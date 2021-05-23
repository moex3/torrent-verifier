MultiThread = Yes
InstallPrefix = /usr/local/bin

PROGNAME = torrent-verify
CC = cc
CFLAGS = -Wall -std=gnu11 -I./subm/heapless-bencode -Werror -O3
CPPFLAGS = -DPROGRAM_NAME='"$(PROGNAME)"' -DBUILD_INFO \
		   -DBUILD_HASH="\"`git rev-parse --abbrev-ref HEAD` -> `git rev-parse --short HEAD`\"" -DBUILD_DATE="\"`date -I`\""

ifeq ($(MultiThread), Yes)
CFLAGS += -lpthread
CPPFLAGS += -DMT
endif

SOURCE =  $(wildcard subm/heapless-bencode/*.c) $(wildcard src/*.c)
#OBJ = $(addsuffix .o,$(basename $(SOURCE)))
OBJS = $(SOURCE:.c=.o)

all: $(PROGNAME)

install: $(PROGNAME)
	install -s -- $< $(InstallPrefix)/$(PROGNAME)

uninstall:
	rm -f -- $(InstallPrefix)/$(PROGNAME)

$(PROGNAME): $(OBJS)
	$(CC) -o $@ $+ $(CFLAGS) $(CPPFLAGS)

clean:
	-rm -- $(OBJS) $(PROGNAME)
