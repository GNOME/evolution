/*
 * metar.c: Metar decoding routines.
 *
 * Originally written by Papadimitriou Spiros <spapadim+@cs.cmu.ed>
 */

#include <glib.h>

#include <unistd.h>
#include <stdlib.h>
#include <regex.h>
#include <math.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include "e-summary.h"
#include "weather.h"

#include "metar.h"

static regex_t metar_re[RE_NUM];

/* Unit conversions and names */

#define TEMP_F_TO_C(f)  (((f) - 32.0) * 0.555556)
#define TEMP_C_TO_F(c)  (((c) * 1.8) + 32.0)
#define TEMP_UNIT_STR(units)  (((units) == UNITS_IMPERIAL) ? "\260F" : "\260C")

#define WINDSPEED_KNOTS_TO_KPH(knots)  ((knots) * 1.851965)
#define WINDSPEED_KPH_TO_KNOTS(kph)    ((kph) * 0.539967)
#define WINDSPEED_UNIT_STR(units) (((units) == UNITS_IMPERIAL) ? "knots" : "kph")

#define PRESSURE_INCH_TO_MM(inch)   ((inch) * 25.4)
#define PRESSURE_MM_TO_INCH(mm)     ((mm) * 0.03937)
#define PRESSURE_MBAR_TO_INCH(mbar) ((mbar) * 0.02963742)
#define PRESSURE_UNIT_STR(units) (((units) == UNITS_IMPERIAL) ? "inHg" : "mmHg")
#define VISIBILITY_SM_TO_KM(sm)  ((sm) * 1.609344)
#define VISIBILITY_KM_TO_SM(km)  ((km) * 0.621371)
#define VISIBILITY_UNIT_STR(units) (((units) == UNITS_IMPERIAL) ? "miles" : "kilometers")

static const char *sky_str[] = {
	N_("Clear sky"),
	N_("Broken clouds"),
	N_("Scattered clouds"),
	N_("Few clouds"),
	N_("Overcast")
};

const char *
weather_sky_string (Weather *w)
{
	if (w->sky < 0 ||
	    w->sky >= (sizeof (sky_str) / sizeof (char *))) {
		return _("Invalid");
	}
	
	return _(sky_str[(int)w->sky]);
}

static const char *wind_direction_str[] = {
	N_("Variable"),
	N_("North"), N_("North - NorthEast"), N_("Northeast"), N_("East - NorthEast"),
	N_("East"), N_("East - Southeast"), N_("Southeast"), N_("South - Southeast"),
	N_("South"), N_("South - Southwest"), N_("Southwest"), N_("West - Southwest"),
	N_("West"), N_("West - Northwest"), N_("Northwest"), N_("North - Northwest")};

const char *
weather_wind_direction_string (Weather *w)
{
	if (w->wind < 0 ||
	    w->wind >= (sizeof (wind_direction_str) / sizeof (char *))) {
		return _("Invalid");
	}

	return _(wind_direction_str[(int)w->wind]);
}

/*
 * Even though tedious, I switched to a 2D array for weather condition
 * strings, in order to facilitate internationalization, esp. for languages
 * with genders.
 *
 * I tried to come up with logical names for most phenomena, but I'm no
 * meteorologist, so there will undoubtedly be some stupid mistakes.
 * However, combinations that did not seem plausible (eg. I cannot imagine
 * what a "light tornado" may be like ;-) were filled in with "??".  If this
 * ever comes up in the weather conditions field, let me know...
 */

/*
 * Note, magic numbers, when you change the size here, make sure to change
 * the below function so that new values are recognized
 */
static const gchar *conditions_str[24][13] = {
/*                   NONE                         VICINITY                             LIGHT                      MODERATE                      HEAVY                      SHALLOW                      PATCHES                         PARTIAL                      THUNDERSTORM                    BLOWING                      SHOWERS                         DRIFTING                      FREEZING                      */
/*               *******************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************/
/* NONE          */ {"",                          "",                                  "",                        "",                           "",                        "",                          "",                             "",                          "",                             "",                          "",                             "",                           "",                          },
/* DRIZZLE       */ {N_("Drizzle"),               N_("Drizzle in the vicinity"),       N_("Light drizzle"),       N_("Moderate drizzle"),       N_("Heavy drizzle"),       N_("Shallow drizzle"),       N_("Patches of drizzle"),       N_("Partial drizzle"),       N_("Thunderstorm"),             N_("Windy drizzle"),         N_("Showers"),                  N_("Drifting drizzle"),       N_("Freezing drizzle")       },
/* RAIN          */ {N_("Rain"),                  N_("Rain in the vicinity") ,         N_("Light rain"),          N_("Moderate rain"),          N_("Heavy rain"),          N_("Shallow rain"),          N_("Patches of rain"),          N_("Partial rainfall"),      N_("Thunderstorm"),             N_("Blowing rainfall"),      N_("Rain showers"),             N_("Drifting rain"),          N_("Freezing rain")          },
/* SNOW          */ {N_("Snow"),                  N_("Snow in the vicinity") ,         N_("Light snow"),          N_("Moderate snow"),          N_("Heavy snow"),          N_("Shallow snow"),          N_("Patches of snow"),          N_("Partial snowfall"),      N_("Snowstorm"),                N_("Blowing snowfall"),      N_("Snow showers"),             N_("Drifting snow"),          N_("Freezing snow")          },
/* SNOW_GRAINS   */ {N_("Snow grains"),           N_("Snow grains in the vicinity") ,  N_("Light snow grains"),   N_("Moderate snow grains"),   N_("Heavy snow grains"),   N_("Shallow snow grains"),   N_("Patches of snow grains"),   N_("Partial snow grains"),   N_("Snowstorm"),                N_("Blowing snow grains"),   N_("Snow grain showers"),       N_("Drifting snow grains"),   N_("Freezing snow grains")   },
/* ICE_CRYSTALS  */ {N_("Ice crystals"),          N_("Ice crystals in the vicinity") , N_("Few ice crystals"),    N_("Moderate ice crystals"),  N_("Heavy ice crystals"),  "??",                        N_("Patches of ice crystals"),  N_("Partial ice crystals"),  N_("Ice crystal storm"),        N_("Blowing ice crystals"),  N_("Showers of ice crystals"),  N_("Drifting ice crystals"),  N_("Freezing ice crystals")  },
/* ICE_PELLETS   */ {N_("Ice pellets"),           N_("Ice pellets in the vicinity") ,  N_("Few ice pellets"),     N_("Moderate ice pellets"),   N_("Heavy ice pellets"),   N_("Shallow ice pellets"),   N_("Patches of ice pellets"),   N_("Partial ice pellets"),   N_("Ice pellet storm"),         N_("Blowing ice pellets"),   N_("Showers of ice pellets"),   N_("Drifting ice pellets"),   N_("Freezing ice pellets")   },
/* HAIL          */ {N_("Hail"),                  N_("Hail in the vicinity") ,         N_("Light hail"),          N_("Moderate hail"),          N_("Heavy hail"),          N_("Shallow hail"),          N_("Patches of hail"),          N_("Partial hail"),          N_("Hailstorm"),                N_("Blowing hail"),          N_("Hail showers"),             N_("Drifting hail"),          N_("Freezing hail")          },
/* SMALL_HAIL    */ {N_("Small hail"),            N_("Small hail in the vicinity") ,   N_("Light hail"),          N_("Moderate small hail"),    N_("Heavy small hail"),    N_("Shallow small hail"),    N_("Patches of small hail"),    N_("Partial small hail"),    N_("Small hailstorm"),          N_("Blowing small hail"),    N_("Showers of small hail"),    N_("Drifting small hail"),    N_("Freezing small hail")    },
/* PRECIPITATION */ {N_("Unknown precipitation"), N_("Precipitation in the vicinity"), N_("Light precipitation"), N_("Moderate precipitation"), N_("Heavy precipitation"), N_("Shallow precipitation"), N_("Patches of precipitation"), N_("Partial precipitation"), N_("Unknown thunderstorm"),     N_("Blowing precipitation"), N_("Showers, type unknown"),    N_("Drifting precipitation"), N_("Freezing precipitation") },
/* MIST          */ {N_("Mist"),                  N_("Mist in the vicinity") ,         N_("Light mist"),          N_("Moderate mist"),          N_("Thick mist"),          N_("Shallow mist"),          N_("Patches of mist"),          N_("Partial mist"),          "??",                           N_("Mist with wind"),        "??",                           N_("Drifting mist"),          N_("Freezing mist")          },
/* FOG           */ {N_("Fog"),                   N_("Fog in the vicinity") ,          N_("Light fog"),           N_("Moderate fog"),           N_("Thick fog"),           N_("Shallow fog"),           N_("Patches of fog"),           N_("Partial fog"),           "??",                           N_("Fog with wind"),         "??",                           N_("Drifting fog"),           N_("Freezing fog")           },
/* SMOKE         */ {N_("Smoke"),                 N_("Smoke in the vicinity") ,        N_("Thin smoke"),          N_("Moderate smoke"),         N_("Thick smoke"),         N_("Shallow smoke"),         N_("Patches of smoke"),         N_("Partial smoke"),         N_("Smoke w/ thunders"),        N_("Smoke with wind"),       "??",                           N_("Drifting smoke"),         "??"                         },
/* VOLCANIC_ASH  */ {N_("Volcanic ash"),          N_("Volcanic ash in the vicinity") , "??",                      N_("Moderate volcanic ash"),  N_("Thick volcanic ash"),  N_("Shallow volcanic ash"),  N_("Patches of volcanic ash"),  N_("Partial volcanic ash"),  N_("Volcanic ash w/ thunders"), N_("Blowing volcanic ash"),  N_("Showers of volcanic ash "), N_("Drifting volcanic ash"),  N_("Freezing volcanic ash")  },
/* SAND          */ {N_("Sand"),                  N_("Sand in the vicinity") ,         N_("Light sand"),          N_("Moderate sand"),          N_("Heavy sand"),          "??",                        N_("Patches of sand"),          N_("Partial sand"),          "??",                           N_("Blowing sand"),          "",                             N_("Drifting sand"),          "??"                         },
/* HAZE          */ {N_("Haze"),                  N_("Haze in the vicinity") ,         N_("Light haze"),          N_("Moderate haze"),          N_("Thick haze"),          N_("Shallow haze"),          N_("Patches of haze"),          N_("Partial haze"),          "??",                           N_("Haze with wind"),        "??",                           N_("Drifting haze"),          N_("Freezing haze")          },
/* SPRAY         */ {N_("Sprays"),                N_("Sprays in the vicinity") ,       N_("Light sprays"),        N_("Moderate sprays"),        N_("Heavy sprays"),        N_("Shallow sprays"),        N_("Patches of sprays"),        N_("Partial sprays"),        "??",                           N_("Blowing sprays"),        "??",                           N_("Drifting sprays"),        N_("Freezing sprays")        },
/* DUST          */ {N_("Dust"),                  N_("Dust in the vicinity") ,         N_("Light dust"),          N_("Moderate dust"),          N_("Heavy dust"),          "??",                        N_("Patches of dust"),          N_("Partial dust"),          "??",                           N_("Blowing dust"),          "??",                           N_("Drifting dust"),          "??"                         },
/* SQUALL        */ {N_("Squall"),                N_("Squall in the vicinity") ,       N_("Light squall"),        N_("Moderate squall"),        N_("Heavy squall"),        "??",                        "??",                           N_("Partial squall"),        N_("Thunderous squall"),        N_("Blowing squall"),        "??",                           N_("Drifting squall"),        N_("Freezing squall")        },
/* SANDSTORM     */ {N_("Sandstorm"),             N_("Sandstorm in the vicinity") ,    N_("Light standstorm"),    N_("Moderate sandstorm"),     N_("Heavy sandstorm"),     N_("Shallow sandstorm"),     "??",                           N_("Partial sandstorm"),     N_("Thunderous sandstorm"),     N_("Blowing sandstorm"),     "??",                           N_("Drifting sandstorm"),     N_("Freezing sandstorm")     },
/* DUSTSTORM     */ {N_("Duststorm"),             N_("Duststorm in the vicinity") ,    N_("Light duststorm"),     N_("Moderate duststorm"),     N_("Heavy duststorm"),     N_("Shallow duststorm"),     "??",                           N_("Partial duststorm"),     N_("Thunderous duststorm"),     N_("Blowing duststorm"),     "??",                           N_("Drifting duststorm"),     N_("Freezing duststorm")     },
/* FUNNEL_CLOUD  */ {N_("Funnel cloud"),          N_("Funnel cloud in the vicinity") , N_("Light funnel cloud"),  N_("Moderate funnel cloud"),  N_("Thick funnel cloud"),  N_("Shallow funnel cloud"),  N_("Patches of funnel clouds"), N_("Partial funnel clouds"), "??",                           N_("Funnel cloud w/ wind"),  "??",                           N_("Drifting funnel cloud"),  "??"                         },
/* TORNADO       */ {N_("Tornado"),               N_("Tornado in the vicinity") ,      "??",                      N_("Moderate tornado"),       N_("Raging tornado"),      "??",                        "??",                           N_("Partial tornado"),       N_("Thunderous tornado"),       N_("Tornado"),               "??",                           N_("Drifting tornado"),       N_("Freezing tornado")       },
/* DUST_WHIRLS   */ {N_("Dust whirls"),           N_("Dust whirls in the vicinity") ,  N_("Light dust whirls"),   N_("Moderate dust whirls"),   N_("Heavy dust whirls"),   N_("Shallow dust whirls"),   N_("Patches of dust whirls"),   N_("Partial dust whirls"),   "??",                           N_("Blowing dust whirls"),   "??",                           N_("Drifting dust whirls"),   "??"                         }
};

const char *
weather_conditions_string (Weather *w)
{
    if (!w->cond.significant) {
	    return "-";
    } else {
	    if (w->cond.phenomenon >= 0 &&
		w->cond.phenomenon < 24 &&
		w->cond.qualifier >= 0 &&
		w->cond.qualifier < 13) {
		    return _(conditions_str[(int)w->cond.phenomenon][(int)w->cond.qualifier]);
	    } else {
		    return _("Invalid");
	    }
    }
}

char *
weather_temp_string (Weather *w)
{
	char *temp;

	temp = g_strdup_printf ("%.1f%s", w->temp, TEMP_UNIT_STR (w->units));
	return temp;
}

void
metar_init_re (void)
{
	static gboolean initialized = FALSE;
	if (initialized)
		return;
	initialized = TRUE;
	
	regcomp(&metar_re[TIME_RE], TIME_RE_STR, REG_EXTENDED);
	regcomp(&metar_re[WIND_RE], WIND_RE_STR, REG_EXTENDED);
	regcomp(&metar_re[VIS_RE], VIS_RE_STR, REG_EXTENDED);
	regcomp(&metar_re[CLOUD_RE], CLOUD_RE_STR, REG_EXTENDED);
	regcomp(&metar_re[TEMP_RE], TEMP_RE_STR, REG_EXTENDED);
	regcomp(&metar_re[PRES_RE], PRES_RE_STR, REG_EXTENDED);
	regcomp(&metar_re[COND_RE], COND_RE_STR, REG_EXTENDED);
}

static inline gint 
days_in_month (gint month, 
	       gint year)
{
	if (month == 1)
		return ((year % 4) == 0) ? 29 : 28;
	else if (((month <= 6) && (month % 2 == 0)) || ((month >=7) && (month % 2 != 0)))
		return 31;
	else
		return 30;
}

/* FIX - there *must* be a simpler, less stupid way to do this!... */
static time_t 
make_time (gint date, 
	   gint hour, 
	   gint min)
{
	struct tm *tm;
	struct tm tms;
	time_t now;
	gint loc_mday, loc_hour, gm_mday, gm_hour;
	gint hour_diff;  /* local time = UTC - hour_diff */
	gint is_dst;
	
	now = time(NULL);
	
	tm = gmtime(&now);
	gm_mday = tm->tm_mday;
	gm_hour = tm->tm_hour;
	memcpy(&tms, tm, sizeof(struct tm));
	
	tm = localtime(&now);
	loc_mday = tm->tm_mday;
	loc_hour = tm->tm_hour;
	is_dst = tm->tm_isdst;
	
	/* Estimate timezone */
	if (gm_mday == loc_mday)
		hour_diff = gm_hour - loc_hour;
	else
		if ((gm_mday == loc_mday + 1) || ((gm_mday == 1) && (loc_mday >= 27)))
			hour_diff = gm_hour + (24 - loc_hour);
		else
			hour_diff = -((24 - gm_hour) + loc_hour);
	
	/* Make time */
	tms.tm_min  = min;
	tms.tm_sec  = 0;
	tms.tm_hour = hour - hour_diff;
	tms.tm_mday = date;
	tms.tm_isdst = is_dst;
	if (tms.tm_hour < 0) {
		tms.tm_hour += 24;
		--tms.tm_mday;
		if (tms.tm_mday < 1) {
			--tms.tm_mon;
			if (tms.tm_mon < 0) {
				tms.tm_mon = 11;
				--tms.tm_year;
			}
			tms.tm_mday = days_in_month(tms.tm_mon, tms.tm_year + 1900);
		}
	} else if (tms.tm_hour > 23) {
		tms.tm_hour -= 24;
		++tms.tm_mday;
		if (tms.tm_mday > days_in_month(tms.tm_mon, tms.tm_year + 1900)) {
			++tms.tm_mon;
			if (tms.tm_mon > 11) {
				tms.tm_mon = 0;
				++tms.tm_year;
			}
			tms.tm_mday = 1;
		}
	}
	
	return mktime(&tms);
}

gboolean
metar_tok_time (char *token,
		Weather *w)
{
	char sday[3], shr[3], smin[3];
	int day, hour, min;

	if (regexec (&metar_re[TIME_RE], token, 0, NULL, 0) == REG_NOMATCH) {
		return FALSE;
	}

	strncpy(sday, token, 2);
	sday[2] = 0;
	day = atoi (sday);

	strncpy (shr, token + 2, 2);
	shr[2] = 0;
	hour = atoi (shr);

	strncpy (smin, token + 4, 2);
	smin[2] = 0;
	min = atoi (smin);

	w->update = make_time (day, hour, min);
	
	return TRUE;
}

#define CONST_DIGITS "0123456789"

gboolean 
metar_tok_wind (gchar *tokp, 
		Weather *w)
{
	char sdir[4], sspd[4], sgust[4];
	int dir, spd, gust = -1;
	char *gustp;
	
	if (regexec(&metar_re[WIND_RE], tokp, 0, NULL, 0) == REG_NOMATCH)
		return FALSE;
	
	strncpy(sdir, tokp, 3);
	sdir[3] = 0;
	dir = (!strcmp(sdir, "VRB")) ? -1 : atoi(sdir);
	
	memset(sspd, 0, sizeof(sspd));
	strncpy(sspd, tokp+3, strspn(tokp+3, CONST_DIGITS));
	spd = atoi(sspd);
	
	gustp = strchr(tokp, 'G');
	if (gustp) {
		memset(sgust, 0, sizeof(sgust));
		strncpy(sgust, gustp+1, strspn(gustp+1, CONST_DIGITS));
		gust = atoi(sgust);
	}
	
	if ((349 <= dir) && (dir <= 11))
		w->wind = WIND_N;
	else if ((12 <= dir) && (dir <= 33))
		w->wind = WIND_NNE;
	else if ((34 <= dir) && (dir <= 56))
		w->wind = WIND_NE;
	else if ((57 <= dir) && (dir <= 78))
		w->wind = WIND_ENE;
	else if ((79 <= dir) && (dir <= 101))
		w->wind = WIND_E;
	else if ((102 <= dir) && (dir <= 123))
		w->wind = WIND_ESE;
	else if ((124 <= dir) && (dir <= 146))
		w->wind = WIND_SE;
	else if ((147 <= dir) && (dir <= 168))
		w->wind = WIND_SSE;
	else if ((169 <= dir) && (dir <= 191))
		w->wind = WIND_S;
	else if ((192 <= dir) && (dir <= 213))
		w->wind = WIND_SSW;
	else if ((214 <= dir) && (dir <= 236))
		w->wind = WIND_SW;
	else if ((247 <= dir) && (dir <= 258))
		w->wind = WIND_WSW;
	else if ((259 <= dir) && (dir <= 281))
		w->wind = WIND_W;
	else if ((282 <= dir) && (dir <= 303))
		w->wind = WIND_WNW;
	else if ((304 <= dir) && (dir <= 326))
		w->wind = WIND_NW;
	else if ((327 <= dir) && (dir <= 348))
		w->wind = WIND_NNW;
	
	w->windspeed = (ESummaryWeatherWindSpeed)spd;
	
	return TRUE;
}

gboolean 
metar_tok_vis (gchar *tokp, 
	       Weather *w)
{
	char *pfrac, *pend;
	char sval[4];
	int val;
	
	if (regexec(&metar_re[VIS_RE], tokp, 0, NULL, 0) == REG_NOMATCH)
		return FALSE;
	
	pfrac = strchr(tokp, '/');
	pend = strstr(tokp, "SM");
	memset(sval, 0, sizeof(sval));
	
	if (pfrac) {
		strncpy(sval, pfrac + 1, pend - pfrac - 1);
		val = atoi(sval);
		w->visibility = (*tokp == 'M') ? 0.001 : (1.0 / ((ESummaryWeatherVisibility)val));
	} else {
		strncpy(sval, tokp, pend - tokp);
		val = atoi(sval);
		w->visibility = (ESummaryWeatherVisibility)val;
	}
	
	return TRUE;
}

gboolean 
metar_tok_cloud (gchar *tokp, 
		 Weather *w)
{
	char stype[4], salt[4];
	int alt = -1;
	
	if (regexec(&metar_re[CLOUD_RE], tokp, 0, NULL, 0) == REG_NOMATCH)
		return FALSE;
	
	strncpy(stype, tokp, 3);
	stype[3] = 0;
	if (strlen(tokp) == 6) {
		strncpy(salt, tokp+3, 3);
		salt[3] = 0;
		alt = atoi(salt);  /* Altitude - currently unused */
	}
	
	if (!strcmp(stype, "CLR")) {
		w->sky = SKY_CLEAR;
	} else if (!strcmp(stype, "BKN")) {
		w->sky = SKY_BROKEN;
	} else if (!strcmp(stype, "SCT")) {
		w->sky = SKY_SCATTERED;
	} else if (!strcmp(stype, "FEW")) {
		w->sky = SKY_FEW;
	} else if (!strcmp(stype, "OVC")) {
		w->sky = SKY_OVERCAST;
	}
	
	return TRUE;
}

gboolean 
metar_tok_pres (gchar *tokp, 
		Weather *w)
{
	if (regexec(&metar_re[PRES_RE], tokp, 0, NULL, 0) == REG_NOMATCH)
		return FALSE;
	
	if (*tokp == 'A') {
		char sintg[3], sfract[3];
		int intg, fract;
		
		strncpy(sintg, tokp+1, 2);
		sintg[2] = 0;
		intg = atoi(sintg);
		
		strncpy(sfract, tokp+3, 2);
		sfract[2] = 0;
		fract = atoi(sfract);
		
		w->pressure = (ESummaryWeatherPressure)intg + (((ESummaryWeatherPressure)fract)/100.0);
	} else {  /* *tokp == 'Q' */
		gchar spres[5];
		gint pres;
		
		strncpy(spres, tokp+1, 4);
		spres[4] = 0;
		pres = atoi(spres);
		
		w->pressure = PRESSURE_MBAR_TO_INCH((ESummaryWeatherPressure)pres);
	}
	
	return TRUE;
}

/* Relative humidity computation - thanks to <Olof.Oberg@modopaper.modogroup.com> */


static inline gint 
calc_humidity(gdouble temp, 
	      gdouble dewp)
{
	gdouble esat, esurf;
	
	temp = TEMP_F_TO_C(temp);
	dewp = TEMP_F_TO_C(dewp);
	
	esat = 6.11 * pow(10.0, (7.5 * temp) / (237.7 + temp));
	esurf = 6.11 * pow(10.0, (7.5 * dewp) / (237.7 + dewp));
	
	return (gint)((esurf/esat) * 100.0);
}

gboolean 
metar_tok_temp (gchar *tokp, 
		Weather *w)
{
    gchar *ptemp, *pdew, *psep;

    if (regexec(&metar_re[TEMP_RE], tokp, 0, NULL, 0) == REG_NOMATCH)
        return FALSE;

    psep = strchr(tokp, '/');
    *psep = 0;
    ptemp = tokp;
    pdew = psep + 1;

    w->temp = (*ptemp == 'M') ? TEMP_C_TO_F(-atoi(ptemp+1)) :
                                   TEMP_C_TO_F(atoi(ptemp));
    w->dew = (*pdew == 'M') ? TEMP_C_TO_F(-atoi(pdew+1)) :
                                 TEMP_C_TO_F(atoi(pdew));
    w->humidity = calc_humidity(w->temp, w->dew);
    return TRUE;
}

gboolean 
metar_tok_cond (gchar *tokp, 
		Weather *w)
{
	char squal[3], sphen[4];
	char *pphen;
	
	if (regexec(&metar_re[COND_RE], tokp, 0, NULL, 0) == REG_NOMATCH)
		return FALSE;
	
	if ((strlen(tokp) > 3) && ((*tokp == '+') || (*tokp == '-')))
		++tokp;   /* FIX */
	
	if ((*tokp == '+') || (*tokp == '-'))
		pphen = tokp + 1;
	else if (strlen(tokp) < 4)
		pphen = tokp;
	else
		pphen = tokp + 2;
	
	memset(squal, 0, sizeof(squal));
	strncpy(squal, tokp, pphen - tokp);
	squal[pphen - tokp] = 0;
	
	memset(sphen, 0, sizeof(sphen));
	strncpy(sphen, pphen, sizeof(sphen));
	sphen[sizeof(sphen)-1] = '\0';
	
	/* Defaults */
	w->cond.qualifier = QUALIFIER_NONE;
	w->cond.phenomenon = PHENOMENON_NONE;
	w->cond.significant = FALSE;
	
	if (!strcmp(squal, "")) {
		w->cond.qualifier = QUALIFIER_MODERATE;
	} else if (!strcmp(squal, "-")) {
		w->cond.qualifier = QUALIFIER_LIGHT;
	} else if (!strcmp(squal, "+")) {
		w->cond.qualifier = QUALIFIER_HEAVY;
	} else if (!strcmp(squal, "VC")) {
		w->cond.qualifier = QUALIFIER_VICINITY;
	} else if (!strcmp(squal, "MI")) {
		w->cond.qualifier = QUALIFIER_SHALLOW;
	} else if (!strcmp(squal, "BC")) {
		w->cond.qualifier = QUALIFIER_PATCHES;
	} else if (!strcmp(squal, "PR")) {
		w->cond.qualifier = QUALIFIER_PARTIAL;
	} else if (!strcmp(squal, "TS")) {
		w->cond.qualifier = QUALIFIER_THUNDERSTORM;
	} else if (!strcmp(squal, "BL")) {
		w->cond.qualifier = QUALIFIER_BLOWING;
	} else if (!strcmp(squal, "SH")) {
		w->cond.qualifier = QUALIFIER_SHOWERS;
	} else if (!strcmp(squal, "DR")) {
		w->cond.qualifier = QUALIFIER_DRIFTING;
	} else if (!strcmp(squal, "FZ")) {
		w->cond.qualifier = QUALIFIER_FREEZING;
	} else {
		g_return_val_if_fail(FALSE, FALSE);
	}
	
	if (!strcmp(sphen, "DZ")) {
		w->cond.phenomenon = PHENOMENON_DRIZZLE;
	} else if (!strcmp(sphen, "RA")) {
		w->cond.phenomenon = PHENOMENON_RAIN;
	} else if (!strcmp(sphen, "SN")) {
		w->cond.phenomenon = PHENOMENON_SNOW;
	} else if (!strcmp(sphen, "SG")) {
		w->cond.phenomenon = PHENOMENON_SNOW_GRAINS;
	} else if (!strcmp(sphen, "IC")) {
		w->cond.phenomenon = PHENOMENON_ICE_CRYSTALS;
	} else if (!strcmp(sphen, "PE")) {
		w->cond.phenomenon = PHENOMENON_ICE_PELLETS;
	} else if (!strcmp(sphen, "GR")) {
		w->cond.phenomenon = PHENOMENON_HAIL;
	} else if (!strcmp(sphen, "GS")) {
		w->cond.phenomenon = PHENOMENON_SMALL_HAIL;
	} else if (!strcmp(sphen, "UP")) {
		w->cond.phenomenon = PHENOMENON_UNKNOWN_PRECIPITATION;
	} else if (!strcmp(sphen, "BR")) {
		w->cond.phenomenon = PHENOMENON_MIST;
	} else if (!strcmp(sphen, "FG")) {
		w->cond.phenomenon = PHENOMENON_FOG;
	} else if (!strcmp(sphen, "FU")) {
		w->cond.phenomenon = PHENOMENON_SMOKE;
	} else if (!strcmp(sphen, "VA")) {
		w->cond.phenomenon = PHENOMENON_VOLCANIC_ASH;
	} else if (!strcmp(sphen, "SA")) {
		w->cond.phenomenon = PHENOMENON_SAND;
	} else if (!strcmp(sphen, "HZ")) {
		w->cond.phenomenon = PHENOMENON_HAZE;
	} else if (!strcmp(sphen, "PY")) {
		w->cond.phenomenon = PHENOMENON_SPRAY;
	} else if (!strcmp(sphen, "DU")) {
		w->cond.phenomenon = PHENOMENON_DUST;
	} else if (!strcmp(sphen, "SQ")) {
		w->cond.phenomenon = PHENOMENON_SQUALL;
	} else if (!strcmp(sphen, "SS")) {
		w->cond.phenomenon = PHENOMENON_SANDSTORM;
	} else if (!strcmp(sphen, "DS")) {
		w->cond.phenomenon = PHENOMENON_DUSTSTORM;
	} else if (!strcmp(sphen, "PO")) {
		w->cond.phenomenon = PHENOMENON_DUST_WHIRLS;
	} else if (!strcmp(sphen, "+FC")) {
		w->cond.phenomenon = PHENOMENON_TORNADO;
	} else if (!strcmp(sphen, "FC")) {
		w->cond.phenomenon = PHENOMENON_FUNNEL_CLOUD;
	} else {
		g_return_val_if_fail(FALSE, FALSE);
	}
	
	if ((w->cond.qualifier != QUALIFIER_NONE) || (w->cond.phenomenon != PHENOMENON_NONE))
		w->cond.significant = TRUE;
	
	return TRUE;
}
