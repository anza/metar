OBJS = src/main.c src/metar.c src/metar.h
CC = cc
CFLAGS = -Wall -lcurl
OUT = metar

prefix = /usr/local
binprefix =
bindir = $(prefix)/bin
mandir = $(prefix)/share/man/man1

all: metar

metar: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(OUT)
	cat metar.1 | gzip > metar.1.gz

install: 
	install metar $(bindir)
	mkdir -p $(mandir)
	install metar.1.gz $(mandir)

clean:
	\rm metar metar.1.gz
