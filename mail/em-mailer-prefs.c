/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "em-mailer-prefs.h"

#include <gal/util/e-iconv.h>
#include <gtkhtml/gtkhtml-properties.h>
#include <libxml/tree.h>
#include "widgets/misc/e-charset-picker.h"
#include <bonobo/bonobo-generic-factory.h>

#include "mail-config.h"


static void em_mailer_prefs_class_init (EMMailerPrefsClass *class);
static void em_mailer_prefs_init       (EMMailerPrefs *dialog);
static void em_mailer_prefs_finalise   (GObject *obj);

static GtkVBoxClass *parent_class = NULL;

enum {
	HEADER_LIST_NAME_COLUMN, /* displayable name of the header (may be a translation) */
	HEADER_LIST_ENABLED_COLUMN, /* is the header enabled? */
	HEADER_LIST_IS_DEFAULT_COLUMN,  /* is this header a default header, eg From: */
	HEADER_LIST_HEADER_COLUMN, /* the real name of this header */
	HEADER_LIST_N_COLUMNS, 
};

static GType col_types[] = {
	G_TYPE_STRING,
	G_TYPE_BOOLEAN,
	G_TYPE_BOOLEAN,
	G_TYPE_STRING
};

/* temporarily copied from em-format.c */
static const struct {
	const char *name;
	guint32 flags;
} default_headers[] = {
	{ N_("From"), EM_FORMAT_HEADER_BOLD },
	{ N_("Reply-To"), EM_FORMAT_HEADER_BOLD },
	{ N_("To"), EM_FORMAT_HEADER_BOLD },
	{ N_("Cc"), EM_FORMAT_HEADER_BOLD },
	{ N_("Bcc"), EM_FORMAT_HEADER_BOLD },
	{ N_("Subject"), EM_FORMAT_HEADER_BOLD },
	{ N_("Date"), EM_FORMAT_HEADER_BOLD },
	{ N_("Newsgroups"), EM_FORMAT_HEADER_BOLD },
	{ "x-evolution-mailer", 0 }, /* DO NOT translate */
};

#define EM_FORMAT_HEADER_XMAILER "x-evolution-mailer"

/* for empty trash on exit frequency */
static const struct {
	const char *label;
	int days;
} empty_trash_frequency[] = {
	{ N_("Every time"), 0 },
	{ N_("Once per day"), 1 },
	{ N_("Once per week"), 7 },
	{ N_("Once per month"), 30 },
};

GtkType
em_mailer_prefs_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		GTypeInfo type_info = {
			sizeof (EMMailerPrefsClass),
			NULL, NULL,
			(GClassInitFunc) em_mailer_prefs_class_init,
			NULL, NULL,
			sizeof (EMMailerPrefs),
			0,
			(GInstanceInitFunc) em_mailer_prefs_init,
		};
		
		type = g_type_register_static (gtk_vbox_get_type (), "EMMailerPrefs", &type_info, 0);
	}
	
	return type;
}

static void
em_mailer_prefs_class_init (EMMailerPrefsClass *klass)
{
	GObjectClass *object_class;
	
	object_class = (GObjectClass *) klass;
	parent_class = g_type_class_ref (gtk_vbox_get_type ());
	
	object_class->finalize = em_mailer_prefs_finalise;
}

static void
em_mailer_prefs_init (EMMailerPrefs *preferences)
{
	preferences->gconf = mail_config_get_gconf_client ();
}

static void
em_mailer_prefs_finalise (GObject *obj)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) obj;
	
	g_object_unref (prefs->gui);
	
        ((GObjectClass *)(parent_class))->finalize (obj);
}


static void
colorpicker_set_color (GnomeColorPicker *color, const char *str)
{
	GdkColor colour;
	guint32 rgb;
	
	gdk_color_parse (str, &colour);
	rgb = ((colour.red & 0xff00) << 8) | (colour.green & 0xff00) | ((colour.blue & 0xff00) >> 8);
	
	gnome_color_picker_set_i8 (color, (rgb & 0xff0000) >> 16, (rgb & 0xff00) >> 8, rgb & 0xff, 0xff);
}

static guint32
colorpicker_get_color (GnomeColorPicker *color)
{
	guint8 r, g, b, a;
	guint32 rgb = 0;
	
	gnome_color_picker_get_i8 (color, &r, &g, &b, &a);
	
	rgb   = r;
	rgb <<= 8;
	rgb  |= g;
	rgb <<= 8;
	rgb  |= b;
	
	return rgb;
}

static void
citation_color_set (GtkWidget *widget, guint r, guint g, guint b, guint a, EMMailerPrefs *prefs)
{
	guint32 rgb = 0;
	char buf[20];
	
	rgb   = r & 0xff;
	rgb <<= 8;
	rgb  |= g & 0xff;
	rgb <<= 8;
	rgb  |= b & 0xff;
	
	sprintf (buf, "#%06x", rgb & 0xffffff);
	
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/display/citation_colour", buf, NULL);
}

static void
labels_changed (EMMailerPrefs *prefs)
{
	GSList *l, *n, *list = NULL;
	const char *cstring;
	char *string;
	guint32 rgb;
	int i;
	
	for (i = 4; i >= 0; i--) {
		cstring = gtk_entry_get_text (prefs->labels[i].name);
		rgb = colorpicker_get_color (prefs->labels[i].color);
		string = g_strdup_printf ("%s:#%06x", cstring, rgb & 0xffffff);
		list = g_slist_prepend (list, string);
	}
	
	gconf_client_set_list (prefs->gconf, "/apps/evolution/mail/labels", GCONF_VALUE_STRING, list, NULL);
	
	l = list;
	while (l != NULL) {
		n = l->next;
		g_free (l->data);
		g_slist_free_1 (l);
		l = n;
	}
}

static void
label_color_set (GtkWidget *widget, guint r, guint g, guint b, guint a, EMMailerPrefs *prefs)
{
	labels_changed (prefs);
}

static void
label_entry_changed (GtkEntry *entry, EMMailerPrefs *prefs)
{
	labels_changed (prefs);
}

static void
restore_labels_clicked (GtkWidget *widget, gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;
	int i;
	
	for (i = 0; i < 5; i++) {
		gtk_entry_set_text (prefs->labels[i].name, _(label_defaults[i].name));
		colorpicker_set_color (prefs->labels[i].color, label_defaults[i].colour);
	}
}

static void
emmp_header_remove_sensitivity (EMMailerPrefs *prefs)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (prefs->header_list);
	gboolean is_default;

	/* remove button should be sensitive if the currenlty selected entry in the list view 
           is not a default header. if there are no entries, or none is selected, it should be 
           disabled
	*/
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (prefs->header_list_store), &iter, 
				    HEADER_LIST_IS_DEFAULT_COLUMN, &is_default, 
				    -1);
		if (is_default)
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->remove_header), FALSE);
		else
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->remove_header), TRUE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->remove_header), FALSE);
	}
}

static gboolean
emmp_header_is_valid (const char *header)
{
	const char *p = header;
	
	if (header[0] == 0)
		return FALSE;
	
	while (*p) {
		if ((*p == ':') || (*p == ' '))
			return FALSE;
		p++;
	}
	
	return TRUE;
}

static void
emmp_header_add_sensitivity (EMMailerPrefs *prefs)
{
	const char *entry_contents;
	GtkTreeIter iter;
	gboolean valid;
	
	/* the add header button should be sensitive if the text box contains 
	   a valid header string, that is not a duplicate with something already 
	   in the list view
	*/
	entry_contents = gtk_entry_get_text (GTK_ENTRY (prefs->entry_header));
	if (!emmp_header_is_valid (entry_contents)) {
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->add_header), FALSE);
		return;
	}
	
	/* check if this is a duplicate */
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (prefs->header_list_store), &iter);
	while (valid) {
		char *header_name;
		
		gtk_tree_model_get (GTK_TREE_MODEL (prefs->header_list_store), &iter, 
				    HEADER_LIST_HEADER_COLUMN, &header_name, 
				    -1);
		if (g_ascii_strcasecmp (header_name, entry_contents) == 0) {
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->add_header), FALSE);
			return;
		}
		
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (prefs->header_list_store), &iter);
	}
	
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->add_header), TRUE);
}

static void
emmp_save_headers (EMMailerPrefs *prefs)
{
	GSList *header_list;
	GtkTreeIter iter;
	gboolean valid;
	
	/* Headers */
	header_list = NULL;
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (prefs->header_list_store), &iter);
	while (valid) {
		struct _EMMailerPrefsHeader h;
		gboolean enabled;
		char *xml;
		
		gtk_tree_model_get (GTK_TREE_MODEL (prefs->header_list_store), &iter, 
				    HEADER_LIST_HEADER_COLUMN, &h.name, 
				    HEADER_LIST_ENABLED_COLUMN, &enabled, 
				    -1);
		h.enabled = enabled;
		
		if ((xml = em_mailer_prefs_header_to_xml (&h)))
			header_list = g_slist_append (header_list, xml);
		
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (prefs->header_list_store), &iter);
	}
	
	gconf_client_set_list (prefs->gconf, "/apps/evolution/mail/display/headers", GCONF_VALUE_STRING, header_list, NULL);
	g_slist_foreach (header_list, (GFunc) g_free, NULL);
	g_slist_free (header_list);
}

static void
emmp_header_list_enabled_toggled (GtkCellRendererToggle *cell, const char *path_string, EMMailerPrefs *prefs)
{
	GtkTreeModel *model = GTK_TREE_MODEL (prefs->header_list_store);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
	GtkTreeIter iter;
	int enabled;
	
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, HEADER_LIST_ENABLED_COLUMN, &enabled, -1);
	enabled = !enabled;
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, HEADER_LIST_ENABLED_COLUMN,
			    enabled, -1);
	gtk_tree_path_free (path);
	
	emmp_save_headers (prefs);
}

static void
emmp_header_add_header (GtkWidget *widget, EMMailerPrefs *prefs)
{
	GtkTreeModel *model = GTK_TREE_MODEL (prefs->header_list_store);
	GtkTreeIter iter;
	
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 
			    HEADER_LIST_NAME_COLUMN, gtk_entry_get_text (prefs->entry_header), 
			    HEADER_LIST_ENABLED_COLUMN, TRUE, 
			    HEADER_LIST_HEADER_COLUMN, gtk_entry_get_text (prefs->entry_header), 
			    HEADER_LIST_IS_DEFAULT_COLUMN, FALSE, 
			    -1);
	gtk_entry_set_text (prefs->entry_header, "");
	emmp_header_remove_sensitivity (prefs);
	emmp_header_add_sensitivity (prefs);
	
	emmp_save_headers (prefs);
}

static void
emmp_header_remove_header (GtkWidget *button, gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;
	GtkTreeModel *model = GTK_TREE_MODEL (prefs->header_list_store);
	GtkTreeSelection *selection = gtk_tree_view_get_selection (prefs->header_list);
	GtkTreeIter iter;
	
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;
	
	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	emmp_header_remove_sensitivity (prefs);
	
	emmp_save_headers (prefs);
}

static void 
emmp_header_list_row_selected (GtkTreeSelection *selection, gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;
	
	emmp_header_remove_sensitivity (prefs);
}

static void
emmp_header_entry_changed (GtkWidget *entry, gpointer user_data)
{
	EMMailerPrefs *prefs = (EMMailerPrefs *) user_data;
	
	emmp_header_add_sensitivity (prefs);
}

static void
mark_seen_timeout_changed (GtkSpinButton *spin, EMMailerPrefs *prefs)
{
	int timeout;
	
	timeout = (int) (gtk_spin_button_get_value (prefs->timeout) * 1000.0);
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/display/mark_seen_timeout", timeout, NULL);
}

static void
spin_button_init (EMMailerPrefs *prefs, GtkSpinButton *spin, const char *key, float div, GCallback value_changed)
{
	GError *err = NULL;
	double min, max;
	char *mkey, *p;
	int val;
	
	gtk_spin_button_get_range (spin, &min, &max);
	
	mkey = g_alloca (strlen (key) + 5);
	p = g_stpcpy (mkey, key);
	*p++ = '_';
	
	/* see if the admin locked down the min value */
	strcpy (p, "min");
	val = gconf_client_get_int (prefs->gconf, mkey, &err);
	if (err == NULL)
		g_clear_error (&err);
	else
		min = (1.0 * val) / div;
	
	/* see if the admin locked down the max value */
	strcpy (p, "max");
	val = gconf_client_get_int (prefs->gconf, mkey, &err);
	if (err == NULL)
		g_clear_error (&err);
	else
		max = (1.0 * val) / div;
	
	gtk_spin_button_set_range (spin, min, max);
	
	/* get the value */
	val = gconf_client_get_int (prefs->gconf, key, NULL);
	gtk_spin_button_set_value (spin, (1.0 * val) / div);
	
	if (value_changed) {
		g_object_set_data ((GObject *) spin, "key", (void *) key);
		g_signal_connect (spin, "value-changed", value_changed, prefs);
	}
	
	if (!gconf_client_key_is_writable (prefs->gconf, key, NULL))
		gtk_widget_set_sensitive ((GtkWidget *) spin, FALSE);
}

static void
toggle_button_toggled (GtkToggleButton *toggle, EMMailerPrefs *prefs)
{
	const char *key;
	
	key = g_object_get_data ((GObject *) toggle, "key");
	gconf_client_set_bool (prefs->gconf, key, gtk_toggle_button_get_active (toggle), NULL);
}

static void
custom_font_changed (GtkToggleButton *toggle, EMMailerPrefs *prefs)
{
	gboolean use_custom;
	
	use_custom = !gtk_toggle_button_get_active (toggle);
	
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->font_fixed), use_custom);
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->font_variable), use_custom);
	
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/display/fonts/use_custom", use_custom, NULL);
}

static void
font_changed (GnomeFontPicker *fontpicker, const char *arg1, EMMailerPrefs *prefs)
{
	const char *key;
	
	key = g_object_get_data ((GObject *) fontpicker, "key");
	gconf_client_set_string (prefs->gconf, key, gnome_font_picker_get_font_name (fontpicker), NULL);
}

static void
toggle_button_init (EMMailerPrefs *prefs, GtkToggleButton *toggle, int not, const char *key, GCallback toggled)
{
	gboolean bool;
	
	bool = gconf_client_get_bool (prefs->gconf, key, NULL);
	gtk_toggle_button_set_active (toggle, not ? !bool : bool);
	
	if (toggled) {
		g_object_set_data ((GObject *) toggle, "key", (void *) key);
		g_signal_connect (toggle, "toggled", toggled, prefs);
	}
	
	if (!gconf_client_key_is_writable (prefs->gconf, key, NULL))
		gtk_widget_set_sensitive ((GtkWidget *) toggle, FALSE);
}

static void
charset_activate (GtkWidget *item, EMMailerPrefs *prefs)
{
	GtkWidget *menu;
	char *string;
	
	menu = gtk_option_menu_get_menu (prefs->charset);
	if (!(string = e_charset_picker_get_charset (menu)))
		string = g_strdup (e_iconv_locale_charset ());
	
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/display/charset", string, NULL);
	g_free (string);
}

static void
charset_menu_init (EMMailerPrefs *prefs)
{
	GtkWidget *menu, *item;
	GList *items;
	char *buf;
	
	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/charset", NULL);
	menu = e_charset_picker_new (buf && *buf ? buf : e_iconv_locale_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	g_free (buf);
	
	items = GTK_MENU_SHELL (menu)->children;
	while (items) {
		item = items->data;
		g_signal_connect (item, "activate", G_CALLBACK (charset_activate), prefs);
		items = items->next;
	}
	
	if (!gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/charset", NULL))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->charset, FALSE);
}

static void
trash_days_activate (GtkWidget *item, EMMailerPrefs *prefs)
{
	int days;
	
	days = GPOINTER_TO_INT (g_object_get_data ((GObject *) item, "days"));
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/trash/empty_on_exit_days", days, NULL);
}

static void
emmp_empty_trash_init (EMMailerPrefs *prefs)
{
	int locked, days, hist = 0, i;
	GtkWidget *menu, *item;
	
	toggle_button_init (prefs, prefs->empty_trash, FALSE,
			    "/apps/evolution/mail/trash/empty_on_exit",
			    G_CALLBACK (toggle_button_toggled));
	
	days = gconf_client_get_int(prefs->gconf, "/apps/evolution/mail/trash/empty_on_exit_days", NULL);
	menu = gtk_menu_new();
	for (i = 0; i < G_N_ELEMENTS (empty_trash_frequency); i++) {
		if (days >= empty_trash_frequency[i].days)
			hist = i;
		
		item = gtk_menu_item_new_with_label (_(empty_trash_frequency[i].label));
		g_object_set_data ((GObject *) item, "days", GINT_TO_POINTER (empty_trash_frequency[i].days));
		g_signal_connect (item, "activate", G_CALLBACK (trash_days_activate), prefs);
		
		gtk_widget_show (item);
		gtk_menu_shell_append((GtkMenuShell *)menu, item);
	}
	
	gtk_widget_show(menu);
	gtk_option_menu_set_menu((GtkOptionMenu *)prefs->empty_trash_days, menu);
	gtk_option_menu_set_history((GtkOptionMenu *)prefs->empty_trash_days, hist);
	
	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/trash/empty_on_exit_days", NULL);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->empty_trash_days, !locked);
}

static void
http_images_changed (GtkWidget *widget, EMMailerPrefs *prefs)
{
	int when;
	
	if (gtk_toggle_button_get_active (prefs->images_always))
		when = MAIL_CONFIG_HTTP_ALWAYS;
	else if (gtk_toggle_button_get_active (prefs->images_sometimes))
		when = MAIL_CONFIG_HTTP_SOMETIMES;
	else
		when = MAIL_CONFIG_HTTP_NEVER;
	
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/display/load_http_images", when, NULL);
}

static void
notify_type_changed (GtkWidget *widget, EMMailerPrefs *prefs)
{
	int type;
	
	if (gtk_toggle_button_get_active (prefs->notify_not))
		type = MAIL_CONFIG_NOTIFY_NOT;
	else if (gtk_toggle_button_get_active (prefs->notify_beep))
		type = MAIL_CONFIG_NOTIFY_BEEP;
	else
		type = MAIL_CONFIG_NOTIFY_PLAY_SOUND;
	
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/notify/type", type, NULL);
}

static void
notify_sound_changed (GtkWidget *widget, EMMailerPrefs *prefs)
{
	const char *filename;
	GtkWidget *entry;
	
	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (prefs->notify_sound_file));
	filename = gtk_entry_get_text (GTK_ENTRY (entry));
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/notify/sound", filename, NULL);
}

static void
em_mailer_prefs_construct (EMMailerPrefs *prefs)
{
	GSList *list, *header_config_list, *header_add_list, *p;
	GHashTable *default_header_hash;
	GtkWidget *toplevel;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkTreeIter iter;
	char *font, *buf;
	GladeXML *gui;
	gboolean locked;
	int val, i;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "preferences_tab", NULL);
	prefs->gui = gui;
	
	/* get our toplevel widget */
	toplevel = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_ref (toplevel);
	gtk_container_remove (GTK_CONTAINER (toplevel->parent), toplevel);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
	gtk_widget_unref (toplevel);
	
	/* General tab */
	
	/* Message Display */
	prefs->timeout_toggle = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkMarkTimeout"));
	toggle_button_init (prefs, prefs->timeout_toggle, FALSE,
			    "/apps/evolution/mail/display/mark_seen",
			    G_CALLBACK (toggle_button_toggled));
	
	prefs->timeout = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "spinMarkTimeout"));
	spin_button_init (prefs, prefs->timeout,
			  "/apps/evolution/mail/display/mark_seen_timeout",
			  1000.0, G_CALLBACK (mark_seen_timeout_changed));
	
	prefs->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	charset_menu_init (prefs);
	
	prefs->citation_highlight = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkHighlightCitations"));
	toggle_button_init (prefs, prefs->citation_highlight, FALSE,
			    "/apps/evolution/mail/display/mark_citations",
			    G_CALLBACK (toggle_button_toggled));
	
	prefs->citation_color = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerHighlightCitations"));
	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/citation_colour", NULL);
	colorpicker_set_color (prefs->citation_color, buf ? buf : "#737373");
	g_signal_connect (prefs->citation_color, "color-set", G_CALLBACK (citation_color_set), prefs);
	if (!gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/citation_colour", NULL))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->citation_color, FALSE);
	g_free (buf);
	
	/* Deleting Mail */
	prefs->empty_trash = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkEmptyTrashOnExit"));
	prefs->empty_trash_days = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuEmptyTrashDays"));
	emmp_empty_trash_init (prefs);
	
	prefs->confirm_expunge = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkConfirmExpunge"));
	toggle_button_init (prefs, prefs->confirm_expunge, FALSE,
			    "/apps/evolution/mail/prompts/expunge",
			    G_CALLBACK (toggle_button_toggled));
	
	/* New Mail Notification */
	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/notify/type", NULL);
	
	val = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/notify/type", NULL);
	prefs->notify_not = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radNotifyNot"));
	gtk_toggle_button_set_active (prefs->notify_not, val == MAIL_CONFIG_NOTIFY_NOT);
	g_signal_connect (prefs->notify_not, "toggled", G_CALLBACK (notify_type_changed), prefs);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->notify_not, FALSE);
	
	prefs->notify_beep = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radNotifyBeep"));
	gtk_toggle_button_set_active (prefs->notify_beep, val == MAIL_CONFIG_NOTIFY_BEEP);
	g_signal_connect (prefs->notify_beep, "toggled", G_CALLBACK (notify_type_changed), prefs);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->notify_beep, FALSE);
	
	prefs->notify_play_sound = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radNotifyPlaySound"));
	gtk_toggle_button_set_active (prefs->notify_play_sound, val == MAIL_CONFIG_NOTIFY_PLAY_SOUND);
	g_signal_connect (prefs->notify_play_sound, "toggled", G_CALLBACK (notify_type_changed), prefs);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->notify_play_sound, FALSE);
	
	prefs->notify_sound_file = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileNotifyPlaySound"));
	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/notify/sound", NULL);
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->notify_sound_file)), buf ? buf : "");
	g_signal_connect (gnome_file_entry_gtk_entry (prefs->notify_sound_file), "changed",
			  G_CALLBACK (notify_sound_changed), prefs);
	if (!gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/notify/sound", NULL))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->notify_sound_file, FALSE);
	g_free (buf);
	
	/* Mail  Fonts */
	font = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/fonts/monospace", NULL);
	prefs->font_fixed = GNOME_FONT_PICKER (glade_xml_get_widget (gui, "FontFixed"));
	gnome_font_picker_set_font_name (prefs->font_fixed, font);
	g_object_set_data ((GObject *) prefs->font_fixed, "key", "/apps/evolution/mail/display/fonts/monospace");
	g_signal_connect (prefs->font_fixed, "font-set", G_CALLBACK (font_changed), prefs);
	if (!gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/fonts/monospace", NULL))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->font_fixed, FALSE);
	
	font = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/fonts/variable", NULL);
	prefs->font_variable = GNOME_FONT_PICKER (glade_xml_get_widget (gui, "FontVariable"));
	gnome_font_picker_set_font_name (prefs->font_variable, font);
	g_object_set_data ((GObject *) prefs->font_variable, "key", "/apps/evolution/mail/display/fonts/variable");
	g_signal_connect (prefs->font_variable, "font-set", G_CALLBACK (font_changed), prefs);
	if (!gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/fonts/variable", NULL))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->font_variable, FALSE);
	
	prefs->font_share = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radFontUseSame"));
	toggle_button_init (prefs, prefs->font_share, TRUE,
			    "/apps/evolution/mail/display/fonts/use_custom",
			    G_CALLBACK (custom_font_changed));
	custom_font_changed (prefs->font_share, prefs);
	
	/* HTML Mail tab */
	
	/* Loading Images */
	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/load_http_images", NULL);
	
	val = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/display/load_http_images", NULL);
	prefs->images_never = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesNever"));
	gtk_toggle_button_set_active (prefs->images_never, val == MAIL_CONFIG_HTTP_NEVER);
	g_signal_connect (prefs->images_never, "toggled", G_CALLBACK (http_images_changed), prefs);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->images_never, FALSE);
	
	prefs->images_sometimes = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesSometimes"));
	gtk_toggle_button_set_active (prefs->images_sometimes, val == MAIL_CONFIG_HTTP_SOMETIMES);
	g_signal_connect (prefs->images_sometimes, "toggled", G_CALLBACK (http_images_changed), prefs);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->images_sometimes, FALSE);
	
	prefs->images_always = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesAlways"));
	gtk_toggle_button_set_active (prefs->images_always, val == MAIL_CONFIG_HTTP_ALWAYS);
	g_signal_connect (prefs->images_always, "toggled", G_CALLBACK (http_images_changed), prefs);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->images_always, FALSE);
	
	prefs->show_animated = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkShowAnimatedImages"));
	toggle_button_init (prefs, prefs->show_animated, FALSE,
			    "/apps/evolution/mail/display/animate_images",
			    G_CALLBACK (toggle_button_toggled));
	
	prefs->prompt_unwanted_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptWantHTML"));
	toggle_button_init (prefs, prefs->prompt_unwanted_html, FALSE,
			    "/apps/evolution/mail/prompts/unwanted_html",
			    G_CALLBACK (toggle_button_toggled));
	
	/* Labels... */
	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/labels", NULL);
	i = 0;
	list = mail_config_get_labels ();
	while (list != NULL && i < 5) {
		MailConfigLabel *label;
		char *widget_name;
		
		label = list->data;
		
		widget_name = g_strdup_printf ("txtLabel%d", i);
		prefs->labels[i].name = GTK_ENTRY (glade_xml_get_widget (gui, widget_name));
		gtk_widget_set_sensitive ((GtkWidget *) prefs->labels[i].name, !locked);
		g_free (widget_name);
		
		widget_name = g_strdup_printf ("colorLabel%d", i);
		prefs->labels[i].color = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, widget_name));
		gtk_widget_set_sensitive ((GtkWidget *) prefs->labels[i].color, !locked);
		g_free (widget_name);
		
		gtk_entry_set_text (prefs->labels[i].name, label->name);
		g_signal_connect (prefs->labels[i].name, "changed", G_CALLBACK (label_entry_changed), prefs);
		
		colorpicker_set_color (prefs->labels[i].color, label->colour);
		g_signal_connect (prefs->labels[i].color, "color-set", G_CALLBACK (label_color_set), prefs);
		
		i++;
		list = list->next;
	}
	
	prefs->restore_labels = GTK_BUTTON (glade_xml_get_widget (gui, "cmdRestoreLabels"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->restore_labels, !locked);
	g_signal_connect (prefs->restore_labels, "clicked", G_CALLBACK (restore_labels_clicked), prefs);
	
	/* headers */
	locked = !gconf_client_key_is_writable (prefs->gconf, "/apps/evolution/mail/display/headers", NULL);
	
	/* always de-sensitised until the user types something in the entry */
	prefs->add_header = GTK_BUTTON (glade_xml_get_widget (gui, "cmdHeadersAdd"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->add_header, FALSE);
	
	/* always de-sensitised until the user selects a header in the list */
	prefs->remove_header = GTK_BUTTON (glade_xml_get_widget (gui, "cmdHeadersRemove"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->remove_header, FALSE);
	
	prefs->entry_header = GTK_ENTRY (glade_xml_get_widget (gui, "txtHeaders"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->entry_header, !locked);
	
	prefs->header_list = GTK_TREE_VIEW (glade_xml_get_widget (gui, "treeHeaders"));
	gtk_widget_set_sensitive ((GtkWidget *) prefs->header_list, !locked);
	
	selection = gtk_tree_view_get_selection (prefs->header_list);
	g_signal_connect (selection, "changed", G_CALLBACK (emmp_header_list_row_selected), prefs);
	g_signal_connect (prefs->entry_header, "changed", G_CALLBACK (emmp_header_entry_changed), prefs);
	g_signal_connect (prefs->entry_header, "activate", G_CALLBACK (emmp_header_add_header), prefs);
	/* initialise the tree with appropriate headings */
	prefs->header_list_store = gtk_list_store_newv (HEADER_LIST_N_COLUMNS, col_types);
	g_signal_connect (prefs->add_header, "clicked", G_CALLBACK (emmp_header_add_header), prefs);
	g_signal_connect (prefs->remove_header, "clicked", G_CALLBACK (emmp_header_remove_header), prefs);
	gtk_tree_view_set_model (prefs->header_list, GTK_TREE_MODEL (prefs->header_list_store));
	
	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (renderer, "activatable", TRUE, NULL);
	g_signal_connect (renderer, "toggled", G_CALLBACK (emmp_header_list_enabled_toggled), prefs);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (prefs->header_list), -1, 
						     "Enabled", renderer,
						     "active", HEADER_LIST_ENABLED_COLUMN, 
						     NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (prefs->header_list), -1, 
						     "Name", renderer,
						     "text", HEADER_LIST_NAME_COLUMN, 
						     NULL);
	
	/* populated the listview with entries; firstly we add all the default headers, and then 
	   we add read header configuration out of gconf. If a header in gconf is a default header, 
	   we update the enabled flag accordingly
	*/
	header_add_list = NULL;
	default_header_hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < sizeof (default_headers) / sizeof (default_headers[0]); i++) {
		struct _EMMailerPrefsHeader *h;
		
		h = g_malloc (sizeof (struct _EMMailerPrefsHeader));
		h->is_default = TRUE;
		h->name = g_strdup (default_headers[i].name);
		if (g_ascii_strcasecmp (default_headers[i].name, EM_FORMAT_HEADER_XMAILER) == 0)
			h->enabled = FALSE;
		else
			h->enabled = TRUE;
		g_hash_table_insert (default_header_hash, (gpointer) default_headers[i].name, h);
		header_add_list = g_slist_append (header_add_list, h);
	}
	
	/* read stored headers from gconf */
	header_config_list = gconf_client_get_list (prefs->gconf, "/apps/evolution/mail/display/headers", GCONF_VALUE_STRING, NULL);
	p = header_config_list;
	while (p) {
		struct _EMMailerPrefsHeader *h, *def;
		char *xml = (char *) p->data;
		
		h = em_mailer_prefs_header_from_xml (xml);
		if (h) {
			def = g_hash_table_lookup (default_header_hash, h->name);
			if (def) {
				def->enabled = h->enabled;
				em_mailer_prefs_header_free (h);
			} else {
				h->is_default = FALSE;
				header_add_list = g_slist_append (header_add_list, h);
			}
		}
		
		p = p->next;
	}
	
	g_hash_table_destroy (default_header_hash);
	g_slist_foreach (header_config_list, (GFunc) g_free, NULL);
	g_slist_free (header_config_list);
	
	p = header_add_list;
	while (p) {
		struct _EMMailerPrefsHeader *h = (struct _EMMailerPrefsHeader *) p->data;
		const char *name;
		
		if (g_ascii_strcasecmp (h->name, EM_FORMAT_HEADER_XMAILER) == 0)
			name = _("Mailer");
		else
			name = _(h->name);
		
		gtk_list_store_append (prefs->header_list_store, &iter);
		gtk_list_store_set (prefs->header_list_store, &iter, 
				    HEADER_LIST_NAME_COLUMN, name, 
				    HEADER_LIST_ENABLED_COLUMN, h->enabled, 
				    HEADER_LIST_IS_DEFAULT_COLUMN, h->is_default, 
				    HEADER_LIST_HEADER_COLUMN, h->name, 
				    -1);
		
		em_mailer_prefs_header_free (h);
		p = p->next;
	}
	
	g_slist_free (header_add_list);
	
	/* Junk prefs */
	prefs->check_incoming = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkCheckIncomingMail"));
	toggle_button_init (prefs, prefs->check_incoming, FALSE,
			    "/apps/evolution/mail/junk/check_incoming",
			    G_CALLBACK (toggle_button_toggled));
	
	prefs->sa_local_tests_only = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkSALocalTestsOnly"));
	toggle_button_init (prefs, prefs->sa_local_tests_only, FALSE,
			    "/apps/evolution/mail/junk/sa/local_only",
			    G_CALLBACK (toggle_button_toggled));
	
	prefs->sa_use_daemon = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkSAUseDaemon"));
	toggle_button_init (prefs, prefs->sa_use_daemon, FALSE,
			    "/apps/evolution/mail/junk/sa/use_daemon",
			    G_CALLBACK (toggle_button_toggled));
}

GtkWidget *
em_mailer_prefs_new (void)
{
	EMMailerPrefs *new;
	
	new = (EMMailerPrefs *) g_object_new (em_mailer_prefs_get_type (), NULL);
	em_mailer_prefs_construct (new);
	
	return (GtkWidget *) new;
}


static struct _EMMailerPrefsHeader *
emmp_header_from_xmldoc (xmlDocPtr doc)
{
	struct _EMMailerPrefsHeader *h;
	xmlNodePtr root;
	xmlChar *name;
	
	if (doc == NULL)
		return NULL;
	
	root = doc->children;
	if (strcmp (root->name, "header") != 0)
		return NULL;
	
	name = xmlGetProp (root, "name");
	if (name == NULL)
		return NULL;
	
	h = g_malloc0 (sizeof (struct _EMMailerPrefsHeader));
	h->name = g_strdup (name);
	xmlFree (name);
	
	if (xmlHasProp (root, "enabled"))
		h->enabled = 1;
	else
		h->enabled = 0;
	
	return h;
}

/**
 * em_mailer_prefs_header_from_xml
 * @xml: XML configuration data
 *
 * Parses passed XML data, which should be of 
 * the format <header name="foo" enabled />, and 
 * returns a EMMailerPrefs structure, or NULL if there 
 * is an error.
 **/
struct _EMMailerPrefsHeader *
em_mailer_prefs_header_from_xml (const char *xml)
{
	struct _EMMailerPrefsHeader *header;
	xmlDocPtr doc;
	
	if (!(doc = xmlParseDoc ((char *) xml)))
		return NULL;
	
	header = emmp_header_from_xmldoc (doc);
	xmlFreeDoc (doc);
	
	return header;
}

/**
 * em_mailer_prefs_header_free
 * @header: header to free
 *
 * Frees the memory associated with the passed header 
 * structure.
 */
void
em_mailer_prefs_header_free (struct _EMMailerPrefsHeader *header)
{
	if (header == NULL)
		return;
	
	g_free (header->name);
	g_free (header);
}

/**
 * em_mailer_prefs_header_to_xml
 * @header: header from which to generate XML
 *
 * Returns the passed header as a XML structure, 
 * or NULL on error
 */
char *
em_mailer_prefs_header_to_xml (struct _EMMailerPrefsHeader *header)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlChar *xml;
	char *out;
	int size;
	
	g_return_val_if_fail (header != NULL, NULL);
	g_return_val_if_fail (header->name != NULL, NULL);
	
	doc = xmlNewDoc ("1.0");
	
	root = xmlNewDocNode (doc, NULL, "header", NULL);
	xmlSetProp (root, "name", header->name);
	if (header->enabled)
		xmlSetProp (root, "enabled", NULL);
	
	xmlDocSetRootElement (doc, root);
	xmlDocDumpMemory (doc, &xml, &size);
	xmlFreeDoc (doc);
	
	out = g_malloc (size + 1);
	memcpy (out, xml, size);
	out[size] = '\0';
	xmlFree (xml);
	
	return out;
}
