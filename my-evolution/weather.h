/*
 * weather.h
 *
 * Copyright (C) 2001,  Ximian, Inc
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef __WEATHER_H__
#define __WEATHER_H__

#include "e-summary-weather.h"

#include <libgnomevfs/gnome-vfs.h>

typedef struct _Weather {
	char *location;
	char *html;
	char *metar;
	GnomeVFSAsyncHandle *handle;
	GString *string;
	char *buffer;

	ESummary *summary;

	gboolean valid;
	ESummaryWeatherLocation *loc;
	ESummaryWeatherUnits units;
	ESummaryWeatherUpdate update;
	ESummaryWeatherSky sky;
	ESummaryWeatherConditions cond;
	ESummaryWeatherTemperature temp;
	ESummaryWeatherTemperature dew;
	ESummaryWeatherHumidity humidity;
	ESummaryWeatherWindDir wind;
	ESummaryWeatherWindSpeed windspeed;
	ESummaryWeatherPressure pressure;
	ESummaryWeatherVisibility visibility;
	char *forecast;
} Weather;

#endif
