# metar - METAR data fetcher and parser

**metar** fetches *aerodrome routine meteorological reports* (METAR) which
contain information about current weather conditions of specified weather
observation stations and decodes it into more easily readable format.

For reference, the weather phenomena reporting codes this tool understands are listed in [PHENOMENA.md](PHENOMENA.md).

## Requirements
libcurl with development headers, eg. ```libcurl4-openssl-dev``` on Debian.

## Installing

### Linux
Running ```make``` and ```make install``` should suffice. What it
does is basically

    gcc -o metar -lcurl main.c metar.c

and installs the executable and manpage to ```/usr/local```.

### BSD
Tested on FreeBSD with the curl port: /usr/ports/ftp/curl;
```mv Makefile.bsd Makefile```, then run ```make``` and ```make install```.

Alternatively, compile with:

    cc -o metar -I/usr/local/include -L/usr/local/lib -lcurl main.c metar.c

## Manual
In case of problems, man page can manually be formatted and viewed by:

    groff -Tascii -man metar.1 | less

## TODO

* Add automatic mapping between ICAO codes and station name.
* Add timezone support or relative time support ("n minutes ago")

## Copyright
This software is modified from standard Debian package ```metar```.
Original code by Kees Leune 2004-2005, improved by Antti Louko <antti@may.fi>
2010, 2013, 2016.

Published under GNU General Public License.
