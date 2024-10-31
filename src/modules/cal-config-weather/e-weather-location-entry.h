/* Taken from libgweather 3, due to it being gone in the libgweather 4
 *
 * SPDX-FileCopyrightText: (C) 2008 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_WEATHER_LOCATION_ENTRY_H
#define E_WEATHER_LOCATION_ENTRY_H 1

#include <gtk/gtk.h>
#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/gweather.h>
#undef GWEATHER_I_KNOW_THIS_IS_UNSTABLE

typedef struct _EWeatherLocationEntry EWeatherLocationEntry;
typedef struct _EWeatherLocationEntryClass EWeatherLocationEntryClass;
typedef struct _EWeatherLocationEntryPrivate EWeatherLocationEntryPrivate;

#define E_WEATHER_TYPE_LOCATION_ENTRY            (e_weather_location_entry_get_type ())
#define E_WEATHER_LOCATION_ENTRY(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), E_WEATHER_TYPE_LOCATION_ENTRY, EWeatherLocationEntry))
#define E_WEATHER_LOCATION_ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_WEATHER_TYPE_LOCATION_ENTRY, EWeatherLocationEntryClass))
#define E_WEATHER_IS_LOCATION_ENTRY(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), E_WEATHER_TYPE_LOCATION_ENTRY))
#define E_WEATHER_IS_LOCATION_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_WEATHER_TYPE_LOCATION_ENTRY))
#define E_WEATHER_LOCATION_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_WEATHER_TYPE_LOCATION_ENTRY, EWeatherLocationEntryClass))

struct _EWeatherLocationEntry {
	GtkSearchEntry parent;

	/*< private >*/
	EWeatherLocationEntryPrivate *priv;
};

struct _EWeatherLocationEntryClass {
	GtkSearchEntryClass parent_class;
};

GType             e_weather_location_entry_get_type     (void);
GtkWidget        *e_weather_location_entry_new          (GWeatherLocation      *top);
void              e_weather_location_entry_set_location (EWeatherLocationEntry *entry,
							 GWeatherLocation *loc);
GWeatherLocation *e_weather_location_entry_get_location (EWeatherLocationEntry *entry);
gboolean          e_weather_location_entry_has_custom_text
							(EWeatherLocationEntry *entry);
gboolean          e_weather_location_entry_set_city     (EWeatherLocationEntry *entry,
							 const gchar *city_name,
							 const gchar *code);

#endif
