/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* weather.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
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
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifndef __WEATHER_H__
#define __WEATHER_H__

#include "e-summary-weather.h"

#include <libgnomevfs/gnome-vfs.h>

typedef struct _Weather {
	char *location;
	char *html;
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
