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

void e_unicode_init (void);

/*
 * e_utf8_strstrcase
 */

const gchar *e_utf8_strstrcase (const gchar *haystack, const gchar *needle);

gchar *e_utf8_from_gtk_event_key (GtkWidget *widget, guint keyval, const gchar *string);

gchar *e_utf8_from_gtk_string (GtkWidget *widget, const gchar *string);
gchar *e_utf8_from_gtk_string_sized (GtkWidget *widget, const gchar *string, gint bytes);

gchar * e_utf8_to_gtk_string (GtkWidget *widget, const gchar *string);
gchar * e_utf8_to_gtk_string_sized (GtkWidget *widget, const gchar *string, gint bytes);

/*
 * These are simple wrappers that save us some typing
 */

/* NB! This return newly allocated string, not const as gtk+ one */

gchar *e_utf8_gtk_entry_get_text (GtkEntry *entry);
void e_utf8_gtk_entry_set_text (GtkEntry *entry, const gchar *text);

gchar *e_utf8_gtk_editable_get_text (GtkEditable *editable);
void e_utf8_gtk_editable_set_text (GtkEditable *editable, const gchar *text);
gchar *e_utf8_gtk_editable_get_chars (GtkEditable *editable, gint start, gint end);
void e_utf8_gtk_editable_insert_text (GtkEditable *editable, const gchar *text, gint length, gint *position);

GtkWidget *e_utf8_gtk_menu_item_new_with_label (GtkMenu *menu, const gchar *label);

void e_utf8_gtk_clist_set_text (GtkCList *clist, gint row, gint col, const gchar *text);
gint e_utf8_gtk_clist_append (GtkCList *clist, gchar *text[]);

gint g_unichar_to_utf8 (gint c, gchar *outbuf);
guint32 gdk_keyval_to_unicode (guint keysym);

#endif


