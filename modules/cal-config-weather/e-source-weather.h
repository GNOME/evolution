/*
 * e-source-weather.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_SOURCE_WEATHER_H
#define E_SOURCE_WEATHER_H

#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_SOURCE_WEATHER \
	(e_source_weather_get_type ())
#define E_SOURCE_WEATHER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_WEATHER, ESourceWeather))
#define E_SOURCE_WEATHER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_WEATHER, ESourceWeatherClass))
#define E_IS_SOURCE_WEATHER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SOURCE_WEATHER))
#define E_IS_SOURCE_WEATHER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SOURCE_WEATHER))
#define E_SOURCE_WEATHER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_WEATHER, ESourceWeatherClass))

#define E_TYPE_SOURCE_WEATHER_UNITS \
	(e_source_weather_units_get_type ())

#define E_SOURCE_EXTENSION_WEATHER_BACKEND "Weather Backend"

G_BEGIN_DECLS

typedef struct _ESourceWeather ESourceWeather;
typedef struct _ESourceWeatherClass ESourceWeatherClass;
typedef struct _ESourceWeatherPrivate ESourceWeatherPrivate;

struct _ESourceWeather {
	ESourceExtension parent;
	ESourceWeatherPrivate *priv;
};

struct _ESourceWeatherClass {
	ESourceExtensionClass parent_class;
};

typedef enum {
	E_SOURCE_WEATHER_UNITS_FAHRENHEIT = 0,
	E_SOURCE_WEATHER_UNITS_CENTIGRADE,
	E_SOURCE_WEATHER_UNITS_KELVIN
} ESourceWeatherUnits;

GType		e_source_weather_get_type	(void);
void		e_source_weather_type_register	(GTypeModule *type_module);
const gchar *	e_source_weather_get_location	(ESourceWeather *extension);
gchar *		e_source_weather_dup_location	(ESourceWeather *extension);
void		e_source_weather_set_location	(ESourceWeather *extension,
						 const gchar *location);
ESourceWeatherUnits
		e_source_weather_get_units	(ESourceWeather *extension);
void		e_source_weather_set_units	(ESourceWeather *extension,
						 ESourceWeatherUnits units);

GType		e_source_weather_units_get_type		(void);

G_END_DECLS

#endif /* E_SOURCE_WEATHER_H */
