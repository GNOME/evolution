#include <gtk/gtk.h>
#include <glib.h>

#include <libebook/e-book.h>
#include <libebook/e-contact.h>

#include <geoclue/geoclue-geocode.h>
#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#include <clutter-gtk/clutter-gtk.h>

void 
get_min_max (gdouble *min_lat, gdouble *max_lat,
        gdouble *min_lng, gdouble *max_lng,
        gdouble lat, gdouble lng);

GeoclueGeocode *get_geocoder (void);

void add_marker (
        ChamplainLayer *layer,
        gdouble lat, gdouble lng,
        EContact *contact);

GHashTable *get_geoclue_from_address (const EContactAddress* addr);

void init_map (ChamplainView **view, GtkWidget **widget);

void create_map_window (GtkWidget *map_widget, const gchar *title);
