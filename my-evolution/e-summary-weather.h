/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-weather.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Iain Holmes
 */

#ifndef __E_SUMMARY_WEATHER_H__
#define __E_SUMMARY_WEATHER_H__

#include <time.h>

#include "e-summary-type.h"
#include "e-summary-shown.h"

typedef struct _ESummaryWeather ESummaryWeather;

#define WEATHER_LOCATION_NAME_MAX_LEN 100
#define WEATHER_LOCATION_CODE_LEN 4
#define WEATHER_LOCATION_ZONE_LEN 7
#define WEATHER_LOCATION_RADAR_LEN 3

typedef struct _ESummaryWeatherLocation {
	char *name;
	char *code;
	char *zone;
	char *radar;
} ESummaryWeatherLocation;

typedef enum _ESummaryWeatherWindDir {
	WIND_VARIABLE,
	WIND_N,
	WIND_NNE,
	WIND_NE,
	WIND_ENE,
	WIND_E,
	WIND_ESE,
	WIND_SE,
	WIND_SSE,
	WIND_S,
	WIND_SSW,
	WIND_SW,
	WIND_WSW,
	WIND_W,
	WIND_WNW,
	WIND_NW,
	WIND_NNW
} ESummaryWeatherWindDir;

typedef enum _ESummaryWeatherSky {
	SKY_CLEAR,
	SKY_BROKEN,
	SKY_SCATTERED,
	SKY_FEW,
	SKY_OVERCAST
} ESummaryWeatherSky;

typedef enum _ESummaryWeatherConditionPhenomenon {
	   PHENOMENON_NONE,

	   PHENOMENON_DRIZZLE,
	   PHENOMENON_RAIN,
	   PHENOMENON_SNOW,
	   PHENOMENON_SNOW_GRAINS,
	   PHENOMENON_ICE_CRYSTALS,
	   PHENOMENON_ICE_PELLETS,
	   PHENOMENON_HAIL,
	   PHENOMENON_SMALL_HAIL,
	   PHENOMENON_UNKNOWN_PRECIPITATION,
	   
	   PHENOMENON_MIST,
	   PHENOMENON_FOG,
	   PHENOMENON_SMOKE,
	   PHENOMENON_VOLCANIC_ASH,
	   PHENOMENON_SAND,
	   PHENOMENON_HAZE,
	   PHENOMENON_SPRAY,
	   PHENOMENON_DUST,
	   
	   PHENOMENON_SQUALL,
	   PHENOMENON_SANDSTORM,
	   PHENOMENON_DUSTSTORM,
	   PHENOMENON_FUNNEL_CLOUD,
	   PHENOMENON_TORNADO,
	   PHENOMENON_DUST_WHIRLS
} ESummaryWeatherConditionPhenomenon;

typedef enum _ESummaryWeatherConditionQualifier {
	   QUALIFIER_NONE,

	   QUALIFIER_VICINITY,
	   
	   QUALIFIER_LIGHT,
	   QUALIFIER_MODERATE,
	   QUALIFIER_HEAVY,
	   QUALIFIER_SHALLOW,
	   QUALIFIER_PATCHES,
	   QUALIFIER_PARTIAL,
	   QUALIFIER_THUNDERSTORM,
	   QUALIFIER_BLOWING,
	   QUALIFIER_SHOWERS,
	   QUALIFIER_DRIFTING,
	   QUALIFIER_FREEZING
} ESummaryWeatherConditionQualifier;

typedef struct _ESummaryWeatherConditions {
	gboolean significant;
	ESummaryWeatherConditionPhenomenon phenomenon;
	ESummaryWeatherConditionQualifier qualifier;
} ESummaryWeatherConditions;

typedef enum _ESummaryWeatherUnits {
	UNITS_IMPERIAL,
	UNITS_METRIC
} ESummaryWeatherUnits;

typedef enum _ESummaryWeatherForecastType {
	FORECAST_STATE,
	FORECAST_ZONE
} ESummaryWeatherForecastType;

typedef double ESummaryWeatherTemperature;
typedef int ESummaryWeatherHumidity;
typedef int ESummaryWeatherWindSpeed;
typedef double ESummaryWeatherPressure;
typedef double ESummaryWeatherVisibility;

typedef time_t ESummaryWeatherUpdate;

char *e_summary_weather_get_html (ESummary *summary);
void e_summary_weather_init (ESummary *summary);
void e_summary_weather_reconfigure (ESummary *summary);
void e_summary_weather_fill_etable (ESummaryShown *ess,
				    ESummary *summary);
const char *e_summary_weather_code_to_name (const char *code);
void e_summary_weather_free (ESummary *summary);
gboolean e_summary_weather_update (ESummary *summary);

#endif
