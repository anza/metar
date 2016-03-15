# metar - METAR fetcher and parser

*metar* fetches the information of current weather conditions of 
specified weather stations and optionally decodes them into more easily 
readable format.

## Requirements
libcurl with development headers, eg. ```libcurl4-openssl-dev``` on Debian.

## Installing

### Linux
Running ```make``` and ```make install``` should suffice. What it
does is basically

    gcc -o metar -lcurl main.c metar.c

and installs the executable and manpage to ```/usr/local```.

### BSD
Tested with FreeBSD.

On FreeBSD libcurl comes with the curl port: /usr/ports/ftp/curl;
Uncomment ```BSDFLAGS``` from Makefile and run ```make```.

Alternatively, compile with:

    cc -o metar -I/usr/local/include -L/usr/local/lib -lcurl main.c metar.c

## Manual
In case of problems, man page can manually be formatted and viewed by:

    groff -Tascii -man metar.1 | less

## Copyright
This software is modified from standard Debian package ```metar```.
Original code by Kees Leune 2004-2005, improved by Antti Louko <antti@may.fi>
2010, 2013, 2016.

Published under GNU General Public License.

