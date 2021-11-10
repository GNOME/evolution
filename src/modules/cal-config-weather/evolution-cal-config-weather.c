/*
 * evolution-cal-config-weather.c
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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/gweather.h>
#undef GWEATHER_I_KNOW_THIS_IS_UNSTABLE

#include "e-weather-location-entry.h"

#include <e-util/e-util.h>

#if defined(HAVE_NL_LANGINFO)
#include <langinfo.h>
#endif

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

	location = g_value_get_object (source_value);
	if (location)
		g_object_ref (location);

	while (location && !gweather_location_has_coords (location)) {
		GWeatherLocation *child = location;

		location = gweather_location_get_parent (child);
		g_object_unref (child);
	}

	if (location) {
		gdouble latitude, longitude;
		gchar lat_str[G_ASCII_DTOSTR_BUF_SIZE + 1];
		gchar lon_str[G_ASCII_DTOSTR_BUF_SIZE + 1];

		gweather_location_get_coords (location, &latitude, &longitude);

		g_ascii_dtostr (lat_str, G_ASCII_DTOSTR_BUF_SIZE, latitude);
		g_ascii_dtostr (lon_str, G_ASCII_DTOSTR_BUF_SIZE, longitude);

		string = g_strdup_printf ("%s/%s", lat_str, lon_str);

		g_object_unref (location);
	}

	g_value_take_string (target_value, string);

	return TRUE;
}

static GWeatherLocation *
cal_config_weather_find_location_by_coords (GWeatherLocation *start,
					    gdouble latitude,
					    gdouble longitude)
{
	GWeatherLocation *location;
	GWeatherLocation *child = NULL;

	if (!start)
		return NULL;

	location = start;
	if (gweather_location_has_coords (location)) {
		gdouble lat, lon;

		gweather_location_get_coords (location, &lat, &lon);

		if (lat == latitude && lon == longitude) {
			g_object_ref (location);
			return location;
		}
	}

	while (child = gweather_location_next_child (location, child), child) {
		GWeatherLocation *result;

		result = cal_config_weather_find_location_by_coords (child, latitude, longitude);
		if (result) {
			g_object_unref (child);
			return result;
		}
	}

	return NULL;
}

static gboolean
cal_config_weather_string_to_location (GBinding *binding,
                                       const GValue *source_value,
                                       GValue *target_value,
                                       gpointer user_data)
{
	GWeatherLocation *world, *match;
	const gchar *string;
	gchar **tokens;
	gdouble latitude, longitude;

	world = user_data;

	string = g_value_get_string (source_value);

	if (string == NULL)
		return FALSE;

	/* String is: latitude '/' longitude */
	tokens = g_strsplit (string, "/", 2);

	if (g_strv_length (tokens) != 2) {
		g_strfreev (tokens);
		return FALSE;
	}

	latitude = g_ascii_strtod (tokens[0], NULL);
	longitude = g_ascii_strtod (tokens[1], NULL);

	match = cal_config_weather_find_location_by_coords (world, latitude, longitude);

	g_value_take_object (target_value, match);

	g_strfreev (tokens);

	return TRUE;
}

static gboolean
is_locale_metric (void)
{
	const gchar *fmt;

#if defined(HAVE_NL_LANGINFO) && defined(HAVE__NL_MEASUREMENT_MEASUREMENT)
	fmt = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);

	if (fmt && *fmt == 2)
		return FALSE;
	else
		return TRUE;
#else
	/* Translators: Please indicate whether your locale uses the
	 * metric or imperial measurement system by changing this to
	 * either "default:mm" or "default:inch", respectively.
	 *
	 * This string is just a fallback mechanism for systems on
	 * which _NL_MEASUREMENT_MEASUREMENT is not available. */
	fmt = C_("locale-metric", "default:mm");

	return g_strcmp0 (fmt, "default:inch") != 0;
#endif
}

static ESourceWeatherUnits
cal_config_weather_get_units_from_locale (void)
{
	return is_locale_metric () ?
		E_SOURCE_WEATHER_UNITS_CENTIGRADE :
		E_SOURCE_WEATHER_UNITS_FAHRENHEIT;
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
	gboolean is_new_source;

	is_new_source = !e_source_has_extension (
		scratch_source, E_SOURCE_EXTENSION_WEATHER_BACKEND);

	context = g_slice_new (Context);
	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) cal_config_weather_context_free);

	world = gweather_location_get_world ();

	widget = e_weather_location_entry_new (world);
	e_source_config_insert_widget (
		config, scratch_source, _("Location:"), widget);
	context->location_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_combo_box_text_new ();
	/* keep the same order as in the ESourceWeatherUnits */
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget),
		/* Translators: This is the temperature in degrees
		 * Fahrenheit. (\302\260 is U+00B0 DEGREE SIGN) */
		_("Fahrenheit (\302\260F)"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget),
		/* Translators: This is the temperature in degrees
		 * Celsius. (\302\260 is U+00B0 DEGREE SIGN) */
		_("Centigrade (\302\260C)"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget),
		/* Translators: This is the temperature in kelvin. */
		_("Kelvin (K)"));
	e_source_config_insert_widget (
		config, scratch_source, _("Units:"), widget);
	gtk_widget_show (widget);

	e_source_config_add_refresh_interval (config, scratch_source);

	extension_name = E_SOURCE_EXTENSION_WEATHER_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	if (is_new_source)
		e_source_weather_set_units (
			E_SOURCE_WEATHER (extension),
			cal_config_weather_get_units_from_locale ());

	e_binding_bind_property_full (
		extension, "location",
		context->location_entry, "location",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		cal_config_weather_string_to_location,
		cal_config_weather_location_to_string,
		g_object_ref (world),
		g_object_unref);

	e_binding_bind_property (
		extension, "units",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	g_object_unref (world);
}

static gboolean
cal_config_weather_check_complete (ESourceConfigBackend *backend,
                                   ESource *scratch_source)
{
	ESourceWeather *extension;
	Context *context;
	gboolean correct;
	const gchar *extension_name;
	const gchar *location;

	context = g_object_get_data (G_OBJECT (backend), e_source_get_uid (scratch_source));
	g_return_val_if_fail (context != NULL, FALSE);

	extension_name = E_SOURCE_EXTENSION_WEATHER_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	location = e_source_weather_get_location (extension);

	g_debug ("Location: [%s]", location);

	correct = (location != NULL) && (*location != '\0');

	e_util_set_entry_issue_hint (context->location_entry, correct ? NULL : _("Location cannot be empty"));

	return correct;
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
	e_cal_config_weather_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
