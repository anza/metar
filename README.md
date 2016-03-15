metar - METAR fetcher and parser
--------------------------------

This software is modified from standard debian package 'metar'.
Original code by Kees Leune 2004-2005.

Published under GNU General Public License.

Requires libcurl!

Compiles on Linux with:
$ gcc -o metar -lcurl main.c metar.c

FreeBSD: libcurl comes with the curl port: /usr/ports/ftp/curl;
Compile with:
$ cc -o metar -I/usr/local/include -L/usr/local/lib -lcurl main.c metar.c
