

#include "gnome-cal.h"
#include <glade/glade.h>
//#include <cal-util/timeutil.h>

void store_to_editable (GladeXML *gui, char *widget_name, gchar *content);
void store_to_toggle (GladeXML *gui, char *widget_name, gboolean content);
void store_to_spin (GladeXML *gui, char *widget_name, gint content);
void store_to_alarm_unit (GladeXML *gui, char *widget_name,
			  enum AlarmUnit content);
void store_to_option (GladeXML *gui, char *widget_name, gint content);
void store_to_gnome_dateedit (GladeXML *gui, char *widget_name,
			      time_t content);


gchar *extract_from_editable (GladeXML *gui, char *widget_name);
gboolean extract_from_toggle (GladeXML *gui, char *widget_name);
gint extract_from_spin (GladeXML *gui, char *widget_name);
enum AlarmUnit extract_from_alarm_unit (GladeXML *gui, char *widget_name);
guint extract_from_option (GladeXML *gui, char *widget_name);
time_t extract_from_gnome_dateedit (GladeXML *gui, char *widget_name);
