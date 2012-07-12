/*
 * evolution-cal-config-weather.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/location-entry.h>
#undef GWEATHER_I_KNOW_THIS_IS_UNSTABLE

#include <misc/e-cal-source-config.h>
#include <misc/e-source-config-backend.h>

#include "e-source-weather.h"

typedef ESourceConfigBackend ECalConfigWeather;
typedef ESourceConfigBackendClass ECalConfigWeatherClass;

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

typedef struct _Context Context;

struct _Context {
	GtkWidget *location_entry;
};

/* Forward Declarations */
GType e_cal_config_weather_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	ECalConfigWeather,
	e_cal_config_weather,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static void
cal_config_weather_context_free (Context *context)
{
	g_object_unref (context->location_entry);

	g_slice_free (Context, context);
}

static gboolean
cal_config_weather_location_to_string (GBinding *binding,
                                       const GValue *source_value,
                                       GValue *target_value,
                                       gpointer user_data)
{
	GWeatherLocation *location;
	gchar *string = NULL;

	location = g_value_get_boxed (source_value);

	if (location != NULL) {
		const gchar *code;
		gchar *city_name;

		code = gweather_location_get_code (location);
		city_name = gweather_location_get_city_name (location);
		string = g_strdup_printf ("%s/%s", code, city_name);
		g_free (city_name);
	}

	g_value_take_string (target_value, string);

	return TRUE;
}

static gboolean
cal_config_weather_string_to_location (GBinding *binding,
                                       const GValue *source_value,
                                       GValue *target_value,
                                       gpointer user_data)
{
	GWeatherLocation *world;
	GWeatherLocation *match, *start;
	const gchar *string;
	const gchar *city_name;
	const gchar *code;
	gchar **tokens;

	world = user_data;

	string = g_value_get_string (source_value);

	if (string == NULL)
		return FALSE;

	/* String is: STATION-CODE '/' CITY-NAME */
	tokens = g_strsplit (string, "/", 2);

	if (g_strv_length (tokens) != 2) {
		g_strfreev (tokens);
		return FALSE;
	}

	code = tokens[0];
	city_name = tokens[1];

	match = start = gweather_location_find_by_station_code (world, code);
	while (match) {
		gchar *cmp_city_name;

		/* Does the city name match? */
		cmp_city_name = gweather_location_get_city_name (match);
		if (g_strcmp0 (city_name, cmp_city_name) == 0) {
			g_free (cmp_city_name);
			break;
		}
		g_free (cmp_city_name);

		/* No match, try parent */
		match = gweather_location_get_parent (match);
	}

	if (match == NULL) {
		/* No exact match, use start instead */
		match = start;
	}

	g_value_set_boxed (target_value, match);

	g_strfreev (tokens);

	return TRUE;
}

static gboolean
cal_config_weather_allow_creation (ESourceConfigBackend *backend)
{
	ESourceConfig *config;
	ECalSourceConfig *cal_config;
	ECalClientSourceType source_type;

	/* No such thing as weather task lists or weather memo lists. */

	config = e_source_config_backend_get_config (backend);

	cal_config = E_CAL_SOURCE_CONFIG (config);
	source_type = e_cal_source_config_get_source_type (cal_config);

	return (source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS);
}

static void
cal_config_weather_insert_widgets (ESourceConfigBackend *backend,
                                   ESource *scratch_source)
{
	ESourceConfig *config;
	ESourceExtension *extension;
	GWeatherLocation *world;
	GtkWidget *widget;
	Context *context;
	const gchar *extension_name;
	const gchar *uid;

	context = g_slice_new (Context);
	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) cal_config_weather_context_free);

	world = gweather_location_new_world (TRUE);

	e_cal_source_config_add_offline_toggle (
		E_CAL_SOURCE_CONFIG (config), scratch_source);

	widget = gweather_location_entry_new (world);
	e_source_config_insert_widget (
		config, scratch_source, _("Location:"), widget);
	context->location_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	e_source_config_add_refresh_interval (config, scratch_source);

	extension_name = E_SOURCE_EXTENSION_WEATHER_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	g_object_bind_property_full (
		extension, "location",
		context->location_entry, "location",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		cal_config_weather_string_to_location,
		cal_config_weather_location_to_string,
		gweather_location_ref (world),
		(GDestroyNotify) gweather_location_unref);

	gweather_location_unref (world);
}

static gboolean
cal_config_weather_check_complete (ESourceConfigBackend *backend,
                                   ESource *scratch_source)
{
	ESourceWeather *extension;
	const gchar *extension_name;
	const gchar *location;

	extension_name = E_SOURCE_EXTENSION_WEATHER_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	location = e_source_weather_get_location (extension);

	g_debug ("Location: [%s]", location);

	return (location != NULL) && (*location != '\0');
}

static void
e_cal_config_weather_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_SOURCE_CONFIG;

	class->parent_uid = "weather-stub";
	class->backend_name = "weather";
	class->allow_creation = cal_config_weather_allow_creation;
	class->insert_widgets = cal_config_weather_insert_widgets;
	class->check_complete = cal_config_weather_check_complete;
}

static void
e_cal_config_weather_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_cal_config_weather_init (ESourceConfigBackend *backend)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_source_weather_type_register (type_module);
	e_cal_config_weather_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
