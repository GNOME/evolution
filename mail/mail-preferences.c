/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include "mail-preferences.h"

#include <gconf/gconf.h>
#include <gal/util/e-iconv.h>
#include <gtkhtml/gtkhtml-properties.h>
#include "widgets/misc/e-charset-picker.h"
#include <bonobo/bonobo-generic-factory.h>

#include "mail-config.h"


static void mail_preferences_class_init (MailPreferencesClass *class);
static void mail_preferences_init       (MailPreferences *dialog);
static void mail_preferences_finalise   (GObject *obj);

static GtkVBoxClass *parent_class = NULL;


GtkType
mail_preferences_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		GTypeInfo type_info = {
			sizeof (MailPreferencesClass),
			NULL, NULL,
			(GClassInitFunc) mail_preferences_class_init,
			NULL, NULL,
			sizeof (MailPreferences),
			0,
			(GInstanceInitFunc) mail_preferences_init,
		};
		
		type = g_type_register_static (gtk_vbox_get_type (), "MailPreferences", &type_info, 0);
	}
	
	return type;
}

static void
mail_preferences_class_init (MailPreferencesClass *klass)
{
	GObjectClass *object_class;
	
	object_class = (GObjectClass *) klass;
	parent_class = g_type_class_ref (gtk_vbox_get_type ());
	
	object_class->finalize = mail_preferences_finalise;
}

static void
mail_preferences_init (MailPreferences *preferences)
{
	preferences->gconf = gconf_client_get_default ();
}

static void
mail_preferences_finalise (GObject *obj)
{
	MailPreferences *prefs = (MailPreferences *) obj;
	
	g_object_unref (prefs->gui);
	g_object_unref (prefs->gconf);
	
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
settings_changed (GtkWidget *widget, gpointer user_data)
{
	MailPreferences *prefs = (MailPreferences *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
font_share_changed (GtkWidget *w, gpointer user_data)
{
	MailPreferences *prefs = (MailPreferences *) user_data;
	gboolean use_custom;

	use_custom = !gtk_toggle_button_get_active (prefs->font_share);

	gtk_widget_set_sensitive (GTK_WIDGET (prefs->font_fixed), use_custom);
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->font_variable), use_custom);

	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
font_changed (GnomeFontPicker *fontpicker, gchar *arg1, gpointer user_data)
{
	MailPreferences *prefs = (MailPreferences *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
color_set (GtkWidget *widget, guint r, guint g, guint b, guint a, gpointer user_data)
{
	MailPreferences *prefs = (MailPreferences *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
restore_labels_clicked (GtkWidget *widget, gpointer user_data)
{
	MailPreferences *prefs = (MailPreferences *) user_data;
	int i;
	
	for (i = 0; i < 5; i++) {
		gtk_entry_set_text (prefs->labels[i].name, _(label_defaults[i].name));
		colorpicker_set_color (prefs->labels[i].color, label_defaults[i].colour);
	}
}

static void
menu_changed (GtkWidget *widget, gpointer user_data)
{
	MailPreferences *prefs = (MailPreferences *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
option_menu_connect (GtkOptionMenu *omenu, gpointer user_data)
{
	GtkWidget *menu, *item;
	GList *items;
	
	menu = gtk_option_menu_get_menu (omenu);
	
	items = GTK_MENU_SHELL (menu)->children;
	while (items) {
		item = items->data;
		g_signal_connect (item, "activate", G_CALLBACK (menu_changed), user_data);
		items = items->next;
	}
}

static void
mail_preferences_construct (MailPreferences *prefs)
{
	GtkWidget *toplevel, *menu;
	GSList *list;
	GladeXML *gui;
	gboolean bool;
	char *font;
	int i, val;
	char *buf;
	
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
	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/display/mark_seen", NULL);
	gtk_toggle_button_set_active (prefs->timeout_toggle, bool);
	g_signal_connect (prefs->timeout_toggle, "toggled", G_CALLBACK (settings_changed), prefs);
	
	prefs->timeout = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "spinMarkTimeout"));
	val = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/display/mark_seen_timeout", NULL);
	gtk_spin_button_set_value (prefs->timeout, (1.0 * val) / 1000.0);
	g_signal_connect (prefs->timeout, "changed", G_CALLBACK (settings_changed), prefs);
	
	prefs->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/format/charset", NULL);
	menu = e_charset_picker_new (buf ? buf : e_iconv_locale_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	option_menu_connect (prefs->charset, prefs);
	g_free (buf);
	
	prefs->citation_highlight = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkHighlightCitations"));
	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/display/mark_citations", NULL);
	gtk_toggle_button_set_active (prefs->citation_highlight, bool);
	g_signal_connect (prefs->citation_highlight, "toggled", G_CALLBACK (settings_changed), prefs);
	
	prefs->citation_color = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerHighlightCitations"));
	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/citation_colour", NULL);
	colorpicker_set_color (prefs->citation_color, buf ? buf : "#737373");
	g_signal_connect (prefs->citation_color, "color-set", G_CALLBACK (color_set), prefs);
	g_free (buf);
	
	/* Deleting Mail */
	prefs->empty_trash = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkEmptyTrashOnExit"));
	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/trash/empty_on_exit", NULL);
	gtk_toggle_button_set_active (prefs->empty_trash, bool);
	g_signal_connect (prefs->empty_trash, "toggled", G_CALLBACK (settings_changed), prefs);
	
	prefs->confirm_expunge = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkConfirmExpunge"));
	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/prompts/expunge", NULL);
	gtk_toggle_button_set_active (prefs->confirm_expunge, bool);
	g_signal_connect (prefs->confirm_expunge, "toggled", G_CALLBACK (settings_changed), prefs);
	
	/* New Mail Notification */
	val = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/notify/type", NULL);
	prefs->notify_not = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radNotifyNot"));
	gtk_toggle_button_set_active (prefs->notify_not, val == MAIL_CONFIG_NOTIFY_NOT);
	g_signal_connect (prefs->notify_not, "toggled", G_CALLBACK (settings_changed), prefs);
	
	prefs->notify_beep = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radNotifyBeep"));
	gtk_toggle_button_set_active (prefs->notify_beep, val == MAIL_CONFIG_NOTIFY_BEEP);
	g_signal_connect (prefs->notify_beep, "toggled", G_CALLBACK (settings_changed), prefs);
	
	prefs->notify_play_sound = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radNotifyPlaySound"));
	gtk_toggle_button_set_active (prefs->notify_play_sound, val == MAIL_CONFIG_NOTIFY_PLAY_SOUND);
	g_signal_connect (prefs->notify_play_sound, "toggled", G_CALLBACK (settings_changed), prefs);
	
	prefs->notify_sound_file = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileNotifyPlaySound"));
	buf = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/notify/sound", NULL);
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->notify_sound_file)), buf ? buf : "");
	g_signal_connect (gnome_file_entry_gtk_entry (prefs->notify_sound_file), "changed",
			  G_CALLBACK (settings_changed), prefs);
	g_free (buf);
	
	/* Mail  Fonts */
	font = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/fonts/monospace", NULL);
	prefs->font_fixed = GNOME_FONT_PICKER (glade_xml_get_widget (gui, "radFontFixed"));
	gnome_font_picker_set_font_name (prefs->font_fixed, font);
	g_signal_connect (prefs->font_fixed, "font-set", G_CALLBACK (font_changed), prefs);
	
	font = gconf_client_get_string (prefs->gconf, "/apps/evolution/mail/display/fonts/variable", NULL);
	prefs->font_variable = GNOME_FONT_PICKER (glade_xml_get_widget (gui, "radFontVariable"));
	gnome_font_picker_set_font_name (prefs->font_variable, font);
	g_signal_connect (prefs->font_variable, "font-set", G_CALLBACK (font_changed), prefs);

	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/display/fonts/use_custom", NULL);
	prefs->font_share = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radFontUseSame"));
	gtk_toggle_button_set_active (prefs->font_share, !bool);
	g_signal_connect (prefs->font_share, "toggled", G_CALLBACK (font_share_changed), prefs);
	font_share_changed (GTK_WIDGET (prefs->font_share), prefs);

	/* HTML Mail tab */
	
	/* Loading Images */

	val = gconf_client_get_int (prefs->gconf, "/apps/evolution/mail/display/load_http_images", NULL);
	prefs->images_never = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesNever"));
	gtk_toggle_button_set_active (prefs->images_never, val == MAIL_CONFIG_HTTP_NEVER);
	g_signal_connect (prefs->images_never, "toggled", G_CALLBACK (settings_changed), prefs);
	
	prefs->images_sometimes = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesSometimes"));
	gtk_toggle_button_set_active (prefs->images_sometimes, val == MAIL_CONFIG_HTTP_SOMETIMES);
	g_signal_connect (prefs->images_sometimes, "toggled", G_CALLBACK (settings_changed), prefs);
	
	prefs->images_always = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesAlways"));
	gtk_toggle_button_set_active (prefs->images_always, val == MAIL_CONFIG_HTTP_ALWAYS);
	g_signal_connect (prefs->images_always, "toggled", G_CALLBACK (settings_changed), prefs);
	
	prefs->show_animated = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkShowAnimatedImages"));
	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/display/animate_images", NULL);
	gtk_toggle_button_set_active (prefs->show_animated, bool);
	g_signal_connect (prefs->show_animated, "toggled", G_CALLBACK (settings_changed), prefs);

	prefs->prompt_unwanted_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptWantHTML"));
	bool = gconf_client_get_bool (prefs->gconf, "/apps/evolution/mail/prompts/unwanted_html", NULL);
	gtk_toggle_button_set_active (prefs->prompt_unwanted_html, bool);
	g_signal_connect (prefs->prompt_unwanted_html, "toggled", G_CALLBACK (settings_changed), prefs);
	
	i = 0;
	list = mail_config_get_labels ();
	while (list != NULL && i < 5) {
		MailConfigLabel *label;
		char *widget_name;
		
		label = list->data;
		
		widget_name = g_strdup_printf ("txtLabel%d", i);
		prefs->labels[i].name = GTK_ENTRY (glade_xml_get_widget (gui, widget_name));
		g_free (widget_name);
		
		widget_name = g_strdup_printf ("colorLabel%d", i);
		prefs->labels[i].color = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, widget_name));
		g_free (widget_name);
		
		gtk_entry_set_text (prefs->labels[i].name, label->name);
		g_signal_connect (prefs->labels[i].name, "changed", G_CALLBACK (settings_changed), prefs);
		
		colorpicker_set_color (prefs->labels[i].color, label->colour);
		g_signal_connect (prefs->labels[i].color, "color_set", G_CALLBACK (color_set), prefs);
		
		i++;
		list = list->next;
	}
	
	prefs->restore_labels = GTK_BUTTON (glade_xml_get_widget (gui, "cmdRestoreLabels"));
	g_signal_connect (prefs->restore_labels, "clicked", G_CALLBACK (restore_labels_clicked), prefs);
}


GtkWidget *
mail_preferences_new (void)
{
	MailPreferences *new;
	
	new = (MailPreferences *) g_object_new (mail_preferences_get_type (), NULL);
	mail_preferences_construct (new);
	
	return (GtkWidget *) new;
}


void
mail_preferences_apply (MailPreferences *prefs)
{
	GtkWidget *entry, *menu;
	char *string, buf[20];
	const char *cstring;
	GSList *list, *l;
	guint32 rgb;
	int i, val;
	
	/* General tab */
	
	/* Message Display */
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/display/mark_seen",
			       gtk_toggle_button_get_active (prefs->timeout_toggle), NULL);
	
	val = (int) (gtk_spin_button_get_value (prefs->timeout) * 1000.0);
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/display/mark_seen_timeout", val, NULL);
	
	menu = gtk_option_menu_get_menu (prefs->charset);
	if (!(string = e_charset_picker_get_charset (menu)))
		string = g_strdup (e_iconv_locale_charset ());
	
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/format/charset", string, NULL);
	g_free (string);
	
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/display/mark_citations",
			       gtk_toggle_button_get_active (prefs->citation_highlight), NULL);
	
	rgb = colorpicker_get_color (prefs->citation_color);
	sprintf (buf,"#%06x", rgb & 0xffffff);
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/display/citation_colour", buf, NULL);
	
	/* Deleting Mail */
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/trash/empty_on_exit",
			       gtk_toggle_button_get_active (prefs->empty_trash), NULL);
	
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/prompts/expunge",
			       gtk_toggle_button_get_active (prefs->confirm_expunge), NULL);
	
	/* New Mail Notification */
	if (gtk_toggle_button_get_active (prefs->notify_not))
		val = MAIL_CONFIG_NOTIFY_NOT;
	else if (gtk_toggle_button_get_active (prefs->notify_beep))
		val = MAIL_CONFIG_NOTIFY_BEEP;
	else
		val = MAIL_CONFIG_NOTIFY_PLAY_SOUND;
	
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/notify/type", val, NULL);
	
	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (prefs->notify_sound_file));
	cstring = gtk_entry_get_text (GTK_ENTRY (entry));
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/notify/sound", cstring, NULL);
	
	/* HTML Mail */
	if (gtk_toggle_button_get_active (prefs->images_always))
		val = MAIL_CONFIG_HTTP_ALWAYS;
	else if (gtk_toggle_button_get_active (prefs->images_sometimes))
		val = MAIL_CONFIG_HTTP_SOMETIMES;
	else
		val = MAIL_CONFIG_HTTP_NEVER;
	
	gconf_client_set_int (prefs->gconf, "/apps/evolution/mail/display/load_http_images", val, NULL);
	
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/display/fonts/variable",
				 gnome_font_picker_get_font_name (prefs->font_variable), NULL);
	gconf_client_set_string (prefs->gconf, "/apps/evolution/mail/display/fonts/monospace",
				 gnome_font_picker_get_font_name (prefs->font_fixed), NULL);
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/display/fonts/use_custom",
			       !gtk_toggle_button_get_active (prefs->font_share), NULL);
	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/display/animate_images",
			       gtk_toggle_button_get_active (prefs->show_animated), NULL);

	gconf_client_set_bool (prefs->gconf, "/apps/evolution/mail/prompts/unwanted_html",
			       gtk_toggle_button_get_active (prefs->prompt_unwanted_html), NULL);
	
	
	/* Labels and Colours */
	list = NULL;
	for (i = 4; i >= 0; i--) {
		cstring = gtk_entry_get_text (prefs->labels[i].name);
		rgb = colorpicker_get_color (prefs->labels[i].color);
		string = g_strdup_printf ("%s:#%06x", cstring, rgb & 0xffffff);
		list = g_slist_prepend (list, string);
	}
	
	gconf_client_set_list (prefs->gconf, "/apps/evolution/mail/labels", GCONF_VALUE_STRING, list, NULL);
	
	l = list;
	while (l != NULL) {
		g_free (l->data);
		l = l->next;
	}
	g_slist_free (list);
	
	gconf_client_suggest_sync (prefs->gconf, NULL);
}
