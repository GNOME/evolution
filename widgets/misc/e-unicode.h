#ifndef _E_UNICODE_H_
#define _E_UNICODE_H_

#include <sys/types.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <config.h>

#ifdef USING_GNOME_PRINT_0_20
#define gnome_font_get_size(f) ((f)->size)
#define gnome_font_get_glyph_width gnome_font_get_width
#define gnome_font_lookup_default gnome_font_get_glyph
#endif

/*
 * e_utf8_strstrcase
 */

const gchar *e_utf8_strstrcase (const gchar *haystack, const gchar *needle);

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

GtkWidget *e_utf8_gtk_menu_item_new_with_label (const gchar *label);

#endif

