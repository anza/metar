OBJS = src/main.c src/metar.c
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

deinstall:
	rm $(bindir)/metar $(mandir)/metar.1.gz

clean:
	\rm metar metar.1.gz
