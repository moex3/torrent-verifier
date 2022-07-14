MultiThread = Yes
HttpTorrent = Yes
InstallPrefix = /usr/local/bin

PROGNAME = torrent-verify
CC = cc
CFLAGS = -Wall -std=gnu11 -I./subm/heapless-bencode -Werror -O3
CPPFLAGS = -DPROGRAM_NAME='"$(PROGNAME)"' -DBUILD_INFO \
		   -DBUILD_HASH="\"`git rev-parse --abbrev-ref HEAD` -> `git rev-parse --short HEAD`\"" -DBUILD_DATE="\"`date -I`\""

ifeq ($(MultiThread), Yes)
LDLIBS += -lpthread
CPPFLAGS += -DMT
endif

ifeq ($(HttpTorrent), Yes)
LDLIBS += -lcurl
CPPFLAGS += -DHTTP_TORRENT=1
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
	$(CC) -o $@ $+ $(CFLAGS) $(CPPFLAGS) $(LDLIBS)


clean:
	-rm -- $(OBJS) $(PROGNAME)
