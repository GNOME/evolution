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

#include "mail-preferences.h"

#include <gal/widgets/e-unicode.h>
#include <gal/util/e-unicode-i18n.h>
#include <gtkhtml/gtkhtml-properties.h>
#include "widgets/misc/e-charset-picker.h"

#include <bonobo/bonobo-generic-factory.h>

#include "mail-config.h"


static void mail_preferences_class_init (MailPreferencesClass *class);
static void mail_preferences_init       (MailPreferences *dialog);
static void mail_preferences_finalise   (GtkObject *obj);

static GtkVBoxClass *parent_class = NULL;


GtkType
mail_preferences_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailPreferences",
			sizeof (MailPreferences),
			sizeof (MailPreferencesClass),
			(GtkClassInitFunc) mail_preferences_class_init,
			(GtkObjectInitFunc) mail_preferences_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_vbox_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_preferences_class_init (MailPreferencesClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;
	parent_class = gtk_type_class (gtk_vbox_get_type ());
	
	object_class->finalize = mail_preferences_finalise;
	/* override methods */
	
}

static void
mail_preferences_init (MailPreferences *preferences)
{
	preferences->gconf = gconf_client_get_default ();
}

static void
mail_preferences_finalise (GtkObject *obj)
{
	MailPreferences *prefs = (MailPreferences *) obj;
	
	gtk_object_unref (GTK_OBJECT (prefs->gui));
	gtk_object_unref (GTK_OBJECT (prefs->pman));
	gtk_object_unref (GTK_OBJECT (prefs->gconf));
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}


static void
colorpicker_set_color (GnomeColorPicker *color, guint32 rgb)
{
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
toggle_button_toggled (GtkWidget *widget, gpointer user_data)
{
	MailPreferences *prefs = (MailPreferences *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
entry_changed (GtkWidget *widget, gpointer user_data)
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
		e_utf8_gtk_entry_set_text (prefs->labels[i].name, U_(label_defaults[i].name));
		colorpicker_set_color (prefs->labels[i].color, label_defaults[i].color);
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
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    menu_changed, user_data);
		items = items->next;
	}
}

static void
mail_preferences_construct (MailPreferences *prefs)
{
	GtkWidget *widget, *toplevel, *menu;
	const char *text;
	GladeXML *gui;
	int i;
	char *names[][2] = {
		{ "anim_check", "chkShowAnimatedImages" },
		{ "magic_links_check", "chkAutoDetectLinks" },
		{ NULL, NULL }
	};
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "preferences_tab");
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
	gtk_toggle_button_set_active (prefs->timeout_toggle, mail_config_get_do_seen_timeout ());
	gtk_signal_connect (GTK_OBJECT (prefs->timeout_toggle), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->timeout = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "spinMarkTimeout"));
	gtk_spin_button_set_value (prefs->timeout, (1.0 * mail_config_get_mark_as_seen_timeout ()) / 1000.0);
	gtk_signal_connect (GTK_OBJECT (prefs->timeout), "changed",
			    entry_changed, prefs);
	
	prefs->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	menu = e_charset_picker_new (mail_config_get_default_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	option_menu_connect (prefs->charset, prefs);
	
	prefs->citation_highlight = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkHighlightCitations"));
	gtk_toggle_button_set_active (prefs->citation_highlight, mail_config_get_citation_highlight ());
	gtk_signal_connect (GTK_OBJECT (prefs->citation_highlight), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->citation_color = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerHighlightCitations"));
	colorpicker_set_color (prefs->citation_color, mail_config_get_citation_color ());
	gtk_signal_connect (GTK_OBJECT (prefs->citation_color), "color-set",
			    color_set, prefs);
	
	/* Deleting Mail */
	prefs->empty_trash = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkEmptyTrashOnExit"));
	gtk_toggle_button_set_active (prefs->empty_trash, mail_config_get_empty_trash_on_exit ());
	gtk_signal_connect (GTK_OBJECT (prefs->empty_trash), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->confirm_expunge = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkConfirmExpunge"));
	gtk_toggle_button_set_active (prefs->confirm_expunge, mail_config_get_confirm_expunge ());
	gtk_signal_connect (GTK_OBJECT (prefs->confirm_expunge), "toggled",
			    toggle_button_toggled, prefs);
	
	/* New Mail Notification */
	prefs->notify_not = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radNotifyNot"));
	gtk_toggle_button_set_active (prefs->notify_not, mail_config_get_new_mail_notify () == MAIL_CONFIG_NOTIFY_NOT);
	gtk_signal_connect (GTK_OBJECT (prefs->notify_not), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->notify_beep = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radNotifyBeep"));
	gtk_toggle_button_set_active (prefs->notify_beep, mail_config_get_new_mail_notify () == MAIL_CONFIG_NOTIFY_BEEP);
	gtk_signal_connect (GTK_OBJECT (prefs->notify_beep), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->notify_play_sound = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radNotifyPlaySound"));
	gtk_toggle_button_set_active (prefs->notify_play_sound,
				      mail_config_get_new_mail_notify () == MAIL_CONFIG_NOTIFY_PLAY_SOUND);
	gtk_signal_connect (GTK_OBJECT (prefs->notify_play_sound), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->notify_sound_file = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileNotifyPlaySound"));
	text = mail_config_get_new_mail_notify_sound_file ();
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->notify_sound_file)),
			    text ? text : "");
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (prefs->notify_sound_file)), "changed",
			    entry_changed, prefs);
	
	/* HTML Mail tab */
	
	/* Loading Images */
	prefs->images_never = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesNever"));
	gtk_toggle_button_set_active (prefs->images_never, mail_config_get_http_mode () == MAIL_CONFIG_HTTP_NEVER);
	gtk_signal_connect (GTK_OBJECT (prefs->images_never), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->images_sometimes = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesSometimes"));
	gtk_toggle_button_set_active (prefs->images_sometimes, mail_config_get_http_mode () == MAIL_CONFIG_HTTP_SOMETIMES);
	gtk_signal_connect (GTK_OBJECT (prefs->images_sometimes), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->images_always = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radImagesAlways"));
	gtk_toggle_button_set_active (prefs->images_always, mail_config_get_http_mode () == MAIL_CONFIG_HTTP_ALWAYS);
	gtk_signal_connect (GTK_OBJECT (prefs->images_always), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->pman = GTK_HTML_PROPMANAGER (gtk_html_propmanager_new (prefs->gconf));
	gtk_signal_connect (GTK_OBJECT (prefs->pman), "changed", toggle_button_toggled, prefs);
	gtk_object_ref (GTK_OBJECT (prefs->pman));
	
	gtk_html_propmanager_set_names (prefs->pman, names);
	gtk_html_propmanager_set_gui (prefs->pman, gui, NULL);
	for (i = 0; names[i][0] != NULL; i++) {
		widget = glade_xml_get_widget (gui, names[i][1]);
		gtk_signal_connect (GTK_OBJECT (widget), "toggled",
				    toggle_button_toggled, prefs);
	}
	
	prefs->prompt_unwanted_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptWantHTML"));
	gtk_toggle_button_set_active (prefs->prompt_unwanted_html, mail_config_get_confirm_unwanted_html ());
	gtk_signal_connect (GTK_OBJECT (prefs->prompt_unwanted_html), "toggled",
			    toggle_button_toggled, prefs);
	
	/* Labels and Colours tab */
	for (i = 0; i < 5; i++) {
		char *widget_name;
		
		widget_name = g_strdup_printf ("txtLabel%d", i);
		prefs->labels[i].name = GTK_ENTRY (glade_xml_get_widget (gui, widget_name));
		g_free (widget_name);
		text = mail_config_get_label_name (i);
		e_utf8_gtk_entry_set_text (prefs->labels[i].name, text ? text : "");
		gtk_signal_connect (GTK_OBJECT (prefs->labels[i].name), "changed",
				    entry_changed, prefs);
		
		widget_name = g_strdup_printf ("colorLabel%d", i);
		prefs->labels[i].color = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, widget_name));
		g_free (widget_name);
		colorpicker_set_color (prefs->labels[i].color, mail_config_get_label_color (i));
		gtk_signal_connect (GTK_OBJECT (prefs->labels[i].color), "color_set",
				    color_set, prefs);
	}
	prefs->restore_labels = GTK_BUTTON (glade_xml_get_widget (gui, "cmdRestoreLabels"));
	gtk_signal_connect (GTK_OBJECT (prefs->restore_labels), "clicked",
			    restore_labels_clicked, prefs);
}


GtkWidget *
mail_preferences_new (void)
{
	MailPreferences *new;
	
	new = (MailPreferences *) gtk_type_new (mail_preferences_get_type ());
	mail_preferences_construct (new);
	
	return (GtkWidget *) new;
}


void
mail_preferences_apply (MailPreferences *prefs)
{
	GtkWidget *entry, *menu;
	char *string;
	guint32 rgb;
	int i, val;
	
	/* General tab */
	
	/* Message Display */
	mail_config_set_do_seen_timeout (gtk_toggle_button_get_active (prefs->timeout_toggle));
	
	val = (int) (gtk_spin_button_get_value_as_float (prefs->timeout) * 1000);
	mail_config_set_mark_as_seen_timeout (val);
	
	menu = gtk_option_menu_get_menu (prefs->charset);
	string = e_charset_picker_get_charset (menu);
	if (string) {
		mail_config_set_default_charset (string);
		g_free (string);
	}
	
	mail_config_set_citation_highlight (gtk_toggle_button_get_active (prefs->citation_highlight));
	
	rgb = colorpicker_get_color (prefs->citation_color);
	mail_config_set_citation_color (rgb);
	
	/* Deleting Mail */
	mail_config_set_empty_trash_on_exit (gtk_toggle_button_get_active (prefs->empty_trash));
	
	mail_config_set_confirm_expunge (gtk_toggle_button_get_active (prefs->confirm_expunge));
	
	/* New Mail Notification */
	if (gtk_toggle_button_get_active (prefs->notify_not))
		mail_config_set_new_mail_notify (MAIL_CONFIG_NOTIFY_NOT);
	else if (gtk_toggle_button_get_active (prefs->notify_beep))
		mail_config_set_new_mail_notify (MAIL_CONFIG_NOTIFY_BEEP);
	else
		mail_config_set_new_mail_notify (MAIL_CONFIG_NOTIFY_PLAY_SOUND);
	
	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (prefs->notify_sound_file));
	string = gtk_entry_get_text (GTK_ENTRY (entry));
	mail_config_set_new_mail_notify_sound_file (string);
	
	/* HTML Mail */
	if (gtk_toggle_button_get_active (prefs->images_always))
		mail_config_set_http_mode (MAIL_CONFIG_HTTP_ALWAYS);
	else if (gtk_toggle_button_get_active (prefs->images_sometimes))
		mail_config_set_http_mode (MAIL_CONFIG_HTTP_SOMETIMES);
	else
		mail_config_set_http_mode (MAIL_CONFIG_HTTP_NEVER);
	
	gtk_html_propmanager_apply (prefs->pman);
	
	mail_config_set_confirm_unwanted_html (gtk_toggle_button_get_active (prefs->prompt_unwanted_html));
	
	/* Labels and Colours */
	for (i = 0; i < 5; i++) {
		/* save the label... */
		string = e_utf8_gtk_entry_get_text (prefs->labels[i].name);
		mail_config_set_label_name (i, string);
		g_free (string);
		
		/* save the colour... */
		rgb = colorpicker_get_color (prefs->labels[i].color);
		mail_config_set_label_color (i, rgb);
	}
	
	mail_config_write ();
}
