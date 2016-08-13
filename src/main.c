/*
  main.c
  metar - metar decoder
  Original author Kees Leune <kees@leune.org> 2004 and 2005
  Further modified by Antti Louko <antti@may.fi> 2010, 2016

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <stdio.h>
#include <ctype.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "metar.h"

/* global variable so we dont have to mess with parameter passing */
char noaabuffer[METAR_MAXSIZE];

/* command line args; everything unset at default */
int rawmetar=0;
int decode=0;
int shortdecode=0;
int verbose=0;
int noconvert=0;
int extra=0;

/** wind unit conversion variables to be made in metar.c **/
 /* from? if this matches, the conversion will be made */
 const char wind_convfrom[5] = "KT";
 /* to? if you change this, make sure your unit fits into char windunit[5]: */
 const char wind_convto[5] = "m/s";
 /* what is the conversion factor? */
 const float wind_convfac = 0.514444;


char *strupc(char *line) {
   char *p;
   for (p=line; *p; p++) *p=toupper(*p);
   return line;
}

/* show brief usage info */
void usage(char *name) {
  printf("metar 1.94 %s %s\n", __DATE__, __TIME__);
  printf("Usage: %s [options] stations\n", name);
  printf("Options\n");
  printf("   -b        decode briefly (default)\n");
  printf("   -d        decode METAR\n");
  printf("   -e        decode briefly with extra information\n");
  printf("   -h        show this help\n");
  printf("   -n        don't convert wind from %s to %s\n",
	 wind_convfrom, wind_convto);
  printf("   -r        print raw METAR data\n");
  printf("   -v        be verbose\n");
  printf("Example: %s -d efjy\n", name);
}


/* place NOAA data in buffer */
int receiveData(void *buffer, size_t size, size_t nmemb, void *stream) {
  size *= nmemb;
  size = (size <= METAR_MAXSIZE) ? size : METAR_MAXSIZE;
  strncpy(noaabuffer, buffer, size);
  return size;
}


/* fetch NOAA report */
int download_Metar(char *station) {
  CURL *curlhandle = NULL;
  CURLcode res;
  char url[URL_MAXSIZE];
  char tmp[URL_MAXSIZE];
  curlhandle = curl_easy_init();
  if (!curlhandle) return 1;

  memset(tmp, 0x0, URL_MAXSIZE);
  if (getenv("METARURL") == NULL) {
    strncpy(tmp, METARURL, URL_MAXSIZE);
  } else {
    strncpy(tmp, getenv("METARURL"), URL_MAXSIZE);
    if (verbose) printf("Using environment variable METARURL: %s\n", tmp);
  }

  if (snprintf(url, URL_MAXSIZE, "%s/%s.TXT", tmp, strupc(station)) < 0)
    return 1;
  if (verbose) printf("Retrieving URL %s\n", url);

  curl_easy_setopt(curlhandle, CURLOPT_URL, url);
  curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, receiveData);
  memset(noaabuffer, 0x0, METAR_MAXSIZE);

  res = curl_easy_perform(curlhandle);
  curl_easy_cleanup(curlhandle);

  if (res == 0) return 0;
  else {
    fprintf(stderr, "CURL error %i while retrieving URL\n", res);
    return 1;
  }
}


/* decode metar */
void decode_Metar(metar_t metar) {
  cloudlist_t *curcloud;
  obslist_t   *curobs;
  stufflist_t *curstuff;
  int n = 0;
  int m = 0;
  double qnh;

  printf("Station       : %s\n", metar.station);
  printf("Day           : %i\n", metar.day);
  printf("Time          : %02i:%02i UTC\n", metar.time/100, metar.time%100);
  if (metar.winddir == -1) {
    printf("Wind direction: Variable\n");
  } else {
    static const char *winddirs[] = {
      "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
      "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    n = ((metar.winddir * 4 + 45) / 90) % 16;
    printf("Wind direction: %i (%s)\n", metar.winddir, winddirs[n]);
  }
  printf("Wind speed    : %.1f %s\n", metar.windstr, metar.windunit);
  if (metar.windstr != metar.windgust) {
  printf("Wind gust     : %.1f %s\n", metar.windgust, metar.windunit);
  }

  /* visibility: treat 9999 m specially */
  if (metar.vis == -1) {
    printf("Visibility    : > 10 km\n");
  } else {
    printf("Visibility    : %i %s\n", metar.vis, metar.visunit);
  }
  printf("Temperature   : %i C\n", metar.temp);
  printf("Dewpoint      : %i C\n", metar.dewp);

  qnh = metar.qnh;
  for (n = 0; n < metar.qnhfp; n++)
    qnh /= 10.0;
  printf("Pressure      : %.*f %s\n", metar.qnhfp, qnh, metar.qnhunit);

  printf("Clouds        : ");
  n = 0;
  for (curcloud = metar.clouds; curcloud != NULL; curcloud=curcloud->next) {
    if (n++ == 0) printf("%s at %d00 ft\n",
			 curcloud->cloud->type, curcloud->cloud->level);
    else printf("%15s %s at %d00 ft\n",
		" ",curcloud->cloud->type, curcloud->cloud->level);
  }
  if (!n) printf("\n");

  printf("Conditions    : ");
  n = 0;
  for (curobs = metar.obs; curobs != NULL; curobs=curobs->next) {
    if (n++ == 0) printf("%s\n", curobs->obs);
    else printf("%15s %s\n", " ",curobs->obs);
  }
  m = 0;
  for (curstuff = metar.stuff; curstuff != NULL; curstuff=curstuff->next) {
    if (m++ == 0) printf("%s\n", curstuff->stuff);
    else printf("%15s %s\n", " ",curstuff->stuff);
  }
    if (!n && !m) printf("\n");
}


/* decode METAR without line breaks */
void shortdecode_Metar(metar_t metar) {
  cloudlist_t *curcloud;
  obslist_t *curobs;
  stufflist_t *curstuff;
  int n = 0;
  double qnh;

  printf("%s day %i time %02i:%02i", metar.station, metar.day, metar.time/100, metar.time%100);
  printf(", temp %i C", metar.temp);
  printf(", ");

  /* if wind gust is different from wind str, include indication */
  if (metar.windgust != metar.windstr) printf("gusty ");

  printf("wind %.1f %s", metar.windstr, metar.windunit);

  if (metar.winddir != -1) {
    static const char *winddirs[] = {
      "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
      "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    n = ((metar.winddir * 4 + 45) / 90) % 16;
    printf(" from %s", winddirs[n]);
  }

  if (extra) {
    qnh = metar.qnh;
    for (n = 0; n < metar.qnhfp; n++)
      qnh /= 10.0;
    printf(", ");
    printf("pressure %.*f %s", metar.qnhfp, qnh, metar.qnhunit);
  }

  /* print observations */
  for (curobs = metar.obs; curobs != NULL; curobs=curobs->next) {
    printf(", %s", curobs->obs);
  }

  if (extra) {
    if (metar.vis == -1) {
      printf(", visibility over 10 km");
    } else {
      printf(", visibility %i %s", metar.vis, metar.visunit);
    }

    for (curstuff = metar.stuff; curstuff != NULL; curstuff=curstuff->next) {
      printf(", %s", curstuff->stuff);
    }

    n = 0;
    for (curcloud = metar.clouds; curcloud != NULL; curcloud=curcloud->next) {
      if (n++ == 0) printf(", clouds: %s at %d00 ft;",
			   curcloud->cloud->type, curcloud->cloud->level);
      else printf(" %s at %d00 ft;",
		  curcloud->cloud->type, curcloud->cloud->level);
    }
  }
  printf("\n");
}


int main(int argc, char* argv[]) {
  int i=0;
  int res=0;
  metar_t metar;
  noaa_t  noaa;

  /* get options */
  opterr=0;
  if (argc == 1) {
    usage(argv[0]);
    return 1;
  }

  while ((res = getopt(argc, argv, "?hvbdern")) != -1) {
    switch (res) {
    case '?':
      usage(argv[0]);
      return 1;
      break;
    case 'h':
      usage(argv[0]);
      return 0;
      break;
    case 'b':
      shortdecode=1;
      break;
    case 'd':
      decode=1;
      break;
    case 'e':
      shortdecode=1;
      extra=1;
      break;
    case 'n':
      noconvert=1;
      break;
    case 'r':
      rawmetar=1;
      break;
    case 'v':
      verbose=1;
      break;
    }
  }

  /* if we aren't given any output options, default to shortdecode */
  if ( !decode && !rawmetar && !shortdecode ) shortdecode = 1;

  curl_global_init(CURL_GLOBAL_DEFAULT);

  // clear out metar and noaa
  memset(&metar, 0x0, sizeof(metar_t));
  memset(&noaa, 0x0, sizeof(noaa_t));


  /* we need at least one parameter if options are given */
  if (optind == argc) {
    usage(argv[0]);
    return 1;
  }

  /* now get metar data from each parameter */
  for (i = optind; i < argc; i++) {

    /* if successfully downloaded... */
    if (download_Metar(argv[i]) == 0) {

      /* ...and parsed NOAA data, parse each METAR report if needed and
	 print stuff out */
      if (parse_NOAA_data(noaabuffer, &noaa) == 0) {
	if (rawmetar) printf("%s", noaa.report);
	if (decode|shortdecode) {
	  parse_Metar(noaa.report, &metar);
	}
	if (decode) {
	  decode_Metar(metar);
	}
	if (shortdecode) {
	  shortdecode_Metar(metar);
	}
      } else {
	/* parse_NOAA_data() returns 1 when station isn't found */
	printf("METAR station %s not found in NOAA data.\n",
		argv[i]);
      }

    } else {
      /* download_Metar() returns 1 and prints the error code of CURL
	 if something has gone wrong */
      printf("METAR data download failed.\n");
    }

  }
  return 0;
}

// EOF
