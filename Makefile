# Uncomment on BSD
#BSD = 1

OBJS = src/main.c src/metar.c
CC = cc
CFLAGS = -Wall -lcurl
OUT = metar

prefix = /usr/local
binprefix =
bindir = $(prefix)/bin

ifeq ($(BSD), 1)
BSDFLAGS = -I/usr/local/include -L/usr/local/lib
mandir = $(prefix)/man/man1
else
mandir = $(prefix)/share/man/man1
endif


all: metar

metar: $(OBJS)
	$(CC) $(CFLAGS) $(BSDFLAGS) $(OBJS) -o $(OUT)
	cat metar.1 | gzip > metar.1.gz

install: 
	install metar $(bindir)
	mkdir -p $(mandir)
	install metar.1.gz $(mandir)

remove:
	rm $(bindir)/metar $(mandir)/metar.1.gz

clean:
	\rm metar metar.1.gz
