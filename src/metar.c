/*
  metar.c
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
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "metar.h"

extern int noconvert;
extern int verbose;
extern int shortdecode;
extern int extra;
extern int decode;

struct observation {
  const char *code;
  const char *description;
};

typedef struct observation observation_t;

struct observation observations[] = {
  /* according to WMO-306 code table 4678 */
  /* proximity */
  {"VC", "vicinity "},
  /* descriptors */
  {"MI", "shallow "},
  {"BC", "patches "},
  {"PR", "partial "},
  {"DR", "low drifting "},
  {"BL", "blowing "},
  {"SH", "showers "},
  {"TS", "thunderstorm "},
  {"FZ", "freezing "},
  /* precipitation */
  {"DZ", "drizzle "},
  {"RA", "rain "},
  {"SN", "snow "},
  {"SG", "snow grains "},
  {"IC", "ice crystals "}, /* for compatibility with FMH-1 */
  {"PL", "ice pellets "},
  {"GR", "hail "},
  {"GS", "small hail/snow pellets "},
  {"UP", "unknown precipitation "},
  /* obscuration */
  {"BR", "mist "},
  {"FG", "fog "},
  {"FU", "smoke "},
  {"VA", "volcanic ash "},
  {"DU", "widespread dust "},
  {"SA", "sand "},
  {"HZ", "haze "},
  {"PY", "spray "}, /* for compatibility with FMH-1 */
  /* other */
  {"PO", "dust/sand whirls "},
  {"SQ", "squalls "},
  {"FC", "funnel cloud "},
  {"SS", "sand storm "},
  {"DS", "dust storm "},
  /* recent */
  {"RE", "recent "}
};


/* Add a cloud to a list of clouds */
static void add_cloud(cloudlist_t **head, cloud_t *cloud) {
  cloudlist_t *current;

  if (*head == NULL) {
    *head = malloc(sizeof(cloudlist_t));
    current = *head;
    current->cloud = cloud;
    current->next = NULL;
    return;
  }
  current = *head;
  while (current->next != NULL)
    current = current->next;

  current->next = (cloudlist_t *)malloc(sizeof(cloudlist_t));
  current = current->next;
  current->cloud = cloud;
  current->next = NULL;
} // add_cloud


/* Add observation */
static void add_observation(obslist_t **head, char *obs) {
  obslist_t *current;

  if (*head == NULL) {
    *head = malloc(sizeof(obslist_t));
    current = *head;
    current->obs = obs;
    current->next = NULL;
    return;
  }
  current = *head;
  while (current->next != NULL)
    current = current->next;

  current->next = (obslist_t *)malloc(sizeof(obslist_t));
  current = current->next;
  current->obs = obs;
  current->next = NULL;
} // add_observation

/* Add observation */
static void add_stuff(stufflist_t **head, char *stuff) {
  stufflist_t *current;

  if (*head == NULL) {
    *head = malloc(sizeof(stufflist_t));
    current = *head;
    current->stuff = stuff;
    current->next = NULL;
    return;
  }
  current = *head;
  while (current->next != NULL)
    current = current->next;

  current->next = (stufflist_t *)malloc(sizeof(stufflist_t));
  current = current->next;
  current->stuff = stuff;
  current->next = NULL;
} // add_observation


/* build the observations regexp patters*/
static void get_observations_pattern(char *pattern, int len) {
  int i=0, p=0;
  int size = sizeof(observations) / sizeof(observation_t);

  for (i=0; i < size; i++) {
    // add observation to string if we have enough space left
    p = strlen(pattern);
    if (p + strlen(observations[i].code) < len)
      strcpy(&pattern[p], observations[i].code);

    // add separator
    p = strlen(pattern);
    if (p < len) pattern[p++] = '|';
  }

  // remove last |
  pattern[strlen(pattern)-1] = 0;
}


/* get the description of code pattern */
static char *decode_obs(char *pattern) {
  int i=0;
  int size = sizeof(observations) / sizeof(observation_t);

  for (i=0; i < size; i++)
    if (strncmp(pattern, observations[i].code, 2) == 0)
      return (char*) observations[i].description;

  return NULL;
}


/* Analyse the token which is provided and, when possible, set the
 * corresponding value in the metar struct
 */
static void analyse_token(char *token, metar_t *metar) {
  regex_t preg;
  regmatch_t pmatch[5];
  int size;
  char tmp[99];
  char obspattern[255];
  char obsp[275];
  static int vis_thresold = 9999;

  extern char wind_convfrom[5];
  extern char wind_convto[5];
  extern float wind_convfac;

  if (verbose) printf("Parsing token `%s'\n", token);

  // find station
  if (metar->station[0] == 0) {
    if (regcomp(&preg, "^([A-Z]+)$", REG_EXTENDED)) {
      perror("parseMetar");
      exit(errno);
    }
    if (!regexec(&preg, token, 5, pmatch, 0)) {
      size = pmatch[1].rm_eo - pmatch[1].rm_so;
      memcpy(metar->station, token+pmatch[1].rm_so,
	     (size < 10 ? size : 10));
      if (verbose) printf("   Found station %s\n", metar->station);
      return;
    }
  }

  // find day/time
  if ((int)metar->day == 0) {
    if (regcomp(&preg, "^([0-9]{2})([0-9]{4})Z$", REG_EXTENDED)) {
      perror("parseMetar");
      exit(errno);
    }
    if (!regexec(&preg, token, 5, pmatch, 0)) {
      size = pmatch[1].rm_eo - pmatch[1].rm_so;
      memset(tmp, 0x0, 99);
      memcpy(tmp, token+pmatch[1].rm_so, (size < 99 ? size : 99));
      sscanf(tmp, "%d", (int*)&metar->day);

      size = pmatch[2].rm_eo - pmatch[2].rm_so;
      memset(tmp, 0x0, 99);
      memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));
      sscanf(tmp, "%d", (int*)&metar->time);
      if (verbose) printf("   Found Day/Time %d/%d\n",
			  metar->day, metar->time);

      return;
    }
  } // daytime

  // find wind
  if ((int)metar->winddir == 0) {
    if (regcomp(&preg, "^(VRB|[0-9]{3})([0-9]{2})(G[0-9]+)?(KT|MPS)$",
		REG_EXTENDED)) {
      perror("parseMetar");
      exit(errno);
    }
    if (!regexec(&preg, token, 5, pmatch, 0)) {
      size = pmatch[1].rm_eo - pmatch[1].rm_so;
      memset(tmp, 0x0, 99);
      if (size) {
	memcpy(tmp, token+pmatch[1].rm_so, (size < 99 ? size : 99));
	if (strstr(tmp, "VRB") == NULL) { // winddir
	  sscanf(tmp, "%d", (int*)&metar->winddir);
	} else { // vrb
	  metar->winddir = -1;
	}
      }

      size = pmatch[2].rm_eo - pmatch[2].rm_so;
      memset(tmp, 0x0, 99);
      if (size) {
	memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));
	sscanf(tmp, "%f", (float*)&metar->windstr);
      }

      size = pmatch[3].rm_eo - pmatch[3].rm_so;
      memset(tmp, 0x0, 99);
      if (size) {
	memcpy(tmp, token+pmatch[3].rm_so+1, (size < 99 ? size-1 : 99));
	sscanf(tmp, "%f", (float*)&metar->windgust);
      } else {
	metar->windgust = metar->windstr;
      }

      size = pmatch[4].rm_eo - pmatch[4].rm_so;
      if (size) {
	memcpy(&metar->windunit, token+pmatch[4].rm_so, (size < 5 ? size : 5));
      }

      /* stuff for converting wind from wind_convfrom to wind_convto */
      /* if noconvert is not specified AND wind unit is knots, do conversion */
      if ( (!noconvert) && (strcmp(metar->windunit, wind_convfrom) == 0) ) {

	/* although these are pointers, their type is float and we can
	 multiply them with another float */
	metar->windstr = (metar->windstr) * wind_convfac;
	metar->windgust = (metar->windgust) * wind_convfac;
	/* let's hope we didn't mess anything up.. */
	strcpy(metar->windunit, wind_convto);
      }

      if (verbose) printf("   Found Winddir/str/gust/unit %d/%f/%f/%s\n",
			  metar->winddir, metar->windstr, metar->windgust,
			  metar->windunit);
      return;
    }
  } // wind

  // find visibility
  if ((int)metar->vis == 0) {
    if (regcomp(&preg, "^([0-9]+)(SM)?$", REG_EXTENDED)) {
      perror("parsemetar");
      exit(errno);
    }
    if (!regexec(&preg, token, 5, pmatch, 0)) {
      size = pmatch[1].rm_eo - pmatch[1].rm_so;
      memset(tmp, 0x0, 99);
      memcpy(tmp, token+pmatch[1].rm_so, (size < 99 ? size : 99));
      sscanf(tmp, "%d", (int*)&metar->vis);

      size = pmatch[2].rm_eo - pmatch[2].rm_so;
      if (size) {
	memset(tmp, 0x0, 99);
	memcpy(&metar->visunit, token+pmatch[2].rm_so,
	       (size < 5 ? size : 5));
      } else
	strncpy(metar->visunit, "m", 1);

      /* return -1 as visibility range if it's 9999 M (>10km) */
      /* it's easier to do it this way because we are fiddling with this
       again at CAVOK and it's easier to check it upon printing from main.c */
      if (metar->vis == vis_thresold) {
	metar->vis = -1;
	if (verbose) printf("   Visibility over 10 km\n");
      }	else if (verbose) {
	printf("   Visibility range/unit %d/%s\n", metar->vis, metar->visunit);
      }
      return;
    }
  } // visibility

  // find temperature and dewpoint
  if ((int)metar->temp == 0) {
    if (regcomp(&preg, "^(M?)([0-9]+)/(M?)([0-9]+)$", REG_EXTENDED)) {
      perror("parsemetar");
      exit(errno);
    }
    if (!regexec(&preg, token, 5, pmatch, 0)) {
      size = pmatch[2].rm_eo - pmatch[2].rm_so;
      memset(tmp, 0x0, 99);
      memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));
      sscanf(tmp, "%d", &metar->temp);

      size = pmatch[1].rm_eo - pmatch[1].rm_so;
      memset(tmp, 0x0, 99);
      memcpy(tmp, token+pmatch[1].rm_so, (size < 99 ? size : 99));
      if (strncmp(tmp, "M", 1) == 0) metar->temp = -metar->temp;

      size = pmatch[4].rm_eo - pmatch[4].rm_so;
      memset(tmp, 0x0, 99);
      memcpy(tmp, token+pmatch[4].rm_so, (size < 99 ? size : 99));
      sscanf(tmp, "%d", &metar->dewp);

      size = pmatch[3].rm_eo - pmatch[3].rm_so;
      memset(tmp, 0x0, 99);
      memcpy(tmp, token+pmatch[3].rm_so, (size < 99 ? size : 99));
      if (strncmp(tmp, "M", 1) == 0) metar->dewp = -metar->dewp;

      if (verbose)
	printf("   Temp/dewpoint %d/%d\n", metar->temp, metar->dewp);
      return;
    }
  } // temp

  // find qnh
  if ((int)metar->qnh == 0) {
    if (regcomp(&preg, "^([QA])([0-9]+)$", REG_EXTENDED)) {
      perror("parsemetar");
      exit(errno);
    }
    if (!regexec(&preg, token, 5, pmatch, 0)) {
      size = pmatch[1].rm_eo - pmatch[1].rm_so;
      memset(tmp, 0x0, 99);
      memcpy(tmp, token+pmatch[1].rm_so, (size < 5 ? size : 5));
      if (strncmp(tmp, "Q", 1) == 0)
	strncpy(metar->qnhunit, "hPa", 3);
      else if (strncmp(tmp, "A", 1) == 0) {
	strncpy(metar->qnhunit, "inHg", 4);
	metar->qnhfp = 2;
      }
      else
	strncpy(metar->qnhunit, "Unkn", 4);

      size = pmatch[2].rm_eo - pmatch[2].rm_so;
      memset(tmp, 0x0, 99);
      memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));
      sscanf(tmp, "%d", &metar->qnh);

      if (verbose)
	printf("   Pressure/unit %d/%s\n", metar->qnh, metar->qnhunit);

      return;
    }
  } // qnh

  // multiple cloud layers possible
  if (regcomp(&preg, "^(VV|SKC|FEW|SCT|BKN|OVC)([0-9]{3})$", REG_EXTENDED)) {
    perror("parsemetar");
    exit(errno);
  }
  if (!regexec(&preg, token, 5, pmatch, 0)) {
    cloud_t *cloud = malloc(sizeof(cloud_t));
    memset(cloud, 0x0, sizeof(cloud_t));

    size=pmatch[1].rm_eo - pmatch[1].rm_so;
    memcpy(&cloud->type, token+pmatch[1].rm_so, (size < 3 ? size : 3));

    size=pmatch[2].rm_eo - pmatch[2].rm_so;
    memset(tmp, 0x0, 99);
    memcpy(tmp, token+pmatch[2].rm_so, (size < 3 ? size : 3));
    sscanf(tmp, "%d", (int*)&cloud->level);

    add_cloud((cloudlist_t **)&metar->clouds, cloud);
    if (verbose)
      printf("   Cloud cover/alt %s/%d00\n", cloud->type, cloud->level);
    return;
  } // cloud

  // phenomena
  memset(obsp, 0x0, 275);
  memset(obspattern, 0x0, 255);
  get_observations_pattern(obspattern, 255);
  snprintf(obsp, 255, "^([+-]?)((%s)+)$", obspattern);


  /* these observations are parsed separately since an algorithm to do
     it would be nasty. these are not exactly weather stuff so i made
     them own struct and that way it's easy to omit these from reports */
  /* it should be easy to add stuff observations afterwards anyway */
  // cannot to CAVOK as an observation in the array because it is more than
  // 2 characters long and that screw up my algorithm
  if (strstr(token, "CAVOK") != NULL) {
    add_stuff((stufflist_t **)&metar->stuff, "ceiling and visibility OK");
    /* yeah, CAVOK means visibility is > 10 km so hit it */
    metar->vis = -1;
    if (verbose) {
      printf("   Ceiling and visibility OK\n");
      printf("   Visibility > 10 km\n");
    }
    return;
  };

  if (strstr(token, "SNOCLO") != NULL) {
    add_stuff((stufflist_t **)&metar->stuff, "aerodrome closed due to snow");
    if (verbose) printf("   Aerodrome closed due to snow\n");
    return;
  };

  if (strstr(token, "NOSIG") != NULL) {
    add_stuff((stufflist_t **)&metar->stuff, "no significant change expected within 2 hours");
    if (verbose) printf("   No significant change expected within 2 hours\n");
    return;
  };

  if (regcomp(&preg, obsp, REG_EXTENDED)) {
    perror("parsemetar");
    exit(errno);
  }
  if (!regexec(&preg, token, 5, pmatch, 0)) {
    char *obs;
    obs = malloc(99);
    memset(obs, 0x0, 99);

    size=pmatch[1].rm_eo - pmatch[1].rm_so;
    memset(tmp, 0x0, 99);
    memcpy(tmp, token+pmatch[1].rm_so, (size < 1 ? size : 1));
    if (tmp[0] == '-') strncpy(obs, "light ", 99);
    else if (tmp[0] == '+') strncpy(obs, "heavy ", 99);

    // split up in groups of 2 chars and decode per group
    size=pmatch[2].rm_eo - pmatch[2].rm_so;
    memset(tmp, 0x0, 99);
    memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));

    int i=0;
    char code[2];
    while (i < strlen(tmp)) {
      memset(code, 0x0, 2);
      memcpy(code, tmp+i, 2);
      strncat(obs, decode_obs(code), 99);
      i += 2;
    }

    // remove trailing space
    obs[strlen(obs)-1]=0;
    add_observation((obslist_t **)&metar->obs, obs);
    if (verbose)
      printf("   Phenomena %s\n", obs);

    return;
  }

  if (verbose) printf("   Unmatched token = %s\n", token);
}


/* PUBLIC--
 * Parse the METAR contain in the report string. Place the parsed report in
 * the metar struct.
 */
void parse_Metar(char *report, metar_t *metar) {
  char *token;
  char *last;

  /* clear results */
  memset(metar, 0x0, sizeof(metar_t));

  // strip trailing newlines
  while ((last = strrchr(report, '\n')) != NULL)
    memset(last, 0, 1);

  token = strtok(report, " ");
  while (token != NULL) {
    analyse_token(token, metar);
    token = strtok(NULL, " ");
  }

} // parse_Metar


/* parse the NOAA report contained in the noaa_data buffer. Place the parsed
 * data in the metar struct.
 */
int parse_NOAA_data(char *noaa_data, noaa_t *noaa) {
  regex_t preg;
  regmatch_t pmatch[10];
  int size;

  if (regcomp(&preg, "^([0-9/]+ [0-9:]+)[[:space:]]+(.*)$", REG_EXTENDED)) {
    fprintf(stderr, "Unable to compile regular expression.\n");
    exit(errno);
  }

  if (regexec(&preg, noaa_data, 10, pmatch, 0)) {
    /* moved to main.c where the return value of this function is checked:
    fprintf(stderr, "METAR pattern not found in NOAA data.\n"); */
    return 1;
  } else {
    memset(noaa, 0x0, sizeof(noaa_t));
    /* date */
    size = pmatch[1].rm_eo - pmatch[1].rm_so;
    memcpy(noaa->date, noaa_data+pmatch[1].rm_so, (size < 36 ? size : 36));

    /* metar */
    size = pmatch[2].rm_eo - pmatch[2].rm_so;
    memcpy(noaa->report, noaa_data+pmatch[2].rm_so,
	   (size < 1024 ? size : 1024));
    return 0;
  }
} // parse_NOAA_data
