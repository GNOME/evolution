#ifndef _E_UNICODE_H_
#define _E_UNICODE_H_

#include <sys/types.h>
#include <glib.h>
#include <gtk/gtk.h>

gchar *e_utf8_from_gtk_event_key (GtkWidget *widget, guint keyval, const gchar *string);

gchar *e_utf8_from_gtk_string (GtkWidget *widget, const gchar *string);

gchar * e_utf8_to_gtk_string (GtkWidget *widget, const gchar *string);

/*
 * These are simple wrappers that save us some typing
 */

/* NB! This return newly allocated string, not const as gtk+ one */

gchar *e_utf8_gtk_entry_get_text (GtkEntry *entry);

void e_utf8_gtk_entry_set_text (GtkEntry *entry, const gchar *text);

gchar *e_utf8_gtk_editable_get_chars (GtkEditable *editable, gint start, gint end);

#endif

