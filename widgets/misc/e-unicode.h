#ifndef _E_UNICODE_H_
#define _E_UNICODE_H_

/*
 * UTF-8 support functions for gal
 *
 * Authors:
 *   Lauris Kaplinski <lauris@helixcode.com>
 *
 * Copyright (C) 2000-2001 Helix Code, Inc.
 *
 */

#include <sys/types.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <config.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#ifdef USING_GNOME_PRINT_0_20
#define gnome_font_get_size(f) ((f)->size)
#define gnome_font_get_glyph_width gnome_font_get_width
#define gnome_font_lookup_default gnome_font_get_glyph
#endif

#define G_UTF8_IN_GAL

void e_unicode_init (void);

/*
 * UTF-8 searching implementations
 *
 * e_utf8_strstrcase - case insensitive search
 * e_utf8_strstrcasedecomp - case insensitive and decompositing search (i.e. accented
 *   letters are treated equal to their base letters, explicit accent marks (unicode
 *   not ascii/iso ones) are ignored).
 */

const gchar *e_utf8_strstrcase (const gchar *haystack, const gchar *needle);
const gchar *e_utf8_strstrcasedecomp (const gchar *haystack, const gchar *needle);

gchar *e_utf8_from_gtk_event_key (GtkWidget *widget, guint keyval, const gchar *string);

gchar *e_utf8_from_gtk_string (GtkWidget *widget, const gchar *string);
gchar *e_utf8_from_gtk_string_sized (GtkWidget *widget, const gchar *string, gint bytes);

gchar *e_utf8_to_gtk_string (GtkWidget *widget, const gchar *string);
gchar *e_utf8_to_gtk_string_sized (GtkWidget *widget, const gchar *string, gint bytes);

gchar *e_utf8_from_locale_string (const gchar *string);
gchar *e_utf8_from_locale_string_sized (const gchar *string, gint bytes);

gchar *e_utf8_to_locale_string (const gchar *string);
gchar *e_utf8_to_locale_string_sized (const gchar *string, gint bytes);
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

gchar * e_utf8_xml1_decode (const gchar *text);
gchar * e_utf8_xml1_encode (const gchar *text);

gint e_unichar_to_utf8 (gint c, gchar *outbuf);
guint32 gdk_keyval_to_unicode (guint keysym);

END_GNOME_DECLS

#endif


