OBJS = src/main.c src/metar.c
CC = cc
CFLAGS = -Wall -lcurl
OUT = metar

# Uncomment on BSD
#BSDFLAGS = -I/usr/local/include -L/usr/local/lib

prefix = /usr/local
binprefix =
bindir = $(prefix)/bin
mandir = $(prefix)/share/man/man1

all: metar

metar: $(OBJS)
	$(CC) $(CFLAGS) $(BSDFLAGS) $(OBJS) -o $(OUT)
	cat metar.1 | gzip > metar.1.gz

install: 
	install metar $(bindir)
	mkdir -p $(mandir)
	install metar.1.gz $(mandir)

clean:
	\rm metar metar.1.gz
