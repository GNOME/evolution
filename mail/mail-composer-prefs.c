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

#include "mail-composer-prefs.h"

#include <bonobo/bonobo-generic-factory.h>

#include "widgets/misc/e-charset-picker.h"

#include "mail-config.h"

static void mail_composer_prefs_class_init (MailComposerPrefsClass *class);
static void mail_composer_prefs_init       (MailComposerPrefs *dialog);
static void mail_composer_prefs_finalise   (GtkObject *obj);

static GtkVBoxClass *parent_class = NULL;


GtkType
mail_composer_prefs_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailComposerPrefs",
			sizeof (MailComposerPrefs),
			sizeof (MailComposerPrefsClass),
			(GtkClassInitFunc) mail_composer_prefs_class_init,
			(GtkObjectInitFunc) mail_composer_prefs_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_vbox_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_composer_prefs_class_init (MailComposerPrefsClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;
	parent_class = gtk_type_class (gtk_vbox_get_type ());
	
	object_class->finalize = mail_composer_prefs_finalise;
	/* override methods */
	
}

static void
mail_composer_prefs_init (MailComposerPrefs *composer_prefs)
{
	;
}

static void
mail_composer_prefs_finalise (GtkObject *obj)
{
	MailComposerPrefs *composer_prefs = (MailComposerPrefs *) obj;
	
	gtk_object_unref (GTK_OBJECT (composer_prefs->gui));
	
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
	
	rgb   = r >> 8;
	rgb <<= 8;
	rgb  |= g >> 8;
	rgb <<= 8;
	rgb  |= b >> 8;
	
	return rgb;
}

static void
attach_style_info (GtkWidget *item, gpointer user_data)
{
	int *style = user_data;
	
	gtk_object_set_data (GTK_OBJECT (item), "style", GINT_TO_POINTER (*style));
	
	(*style)++;
}

static void
toggle_button_toggled (GtkWidget *widget, gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
menu_changed (GtkWidget *widget, gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
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
mail_composer_prefs_construct (MailComposerPrefs *prefs)
{
	GtkWidget *toplevel, *menu;
	GladeXML *gui;
	int style;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "composer_tab");
	prefs->gui = gui;
	
	/* get our toplevel widget */
	toplevel = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_ref (toplevel);
	gtk_container_remove (GTK_CONTAINER (toplevel->parent), toplevel);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
	gtk_widget_unref (toplevel);
	
	/* General tab */
	
	/* Default Behavior */
	prefs->send_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkSendHTML"));
	gtk_toggle_button_set_active (prefs->send_html, mail_config_get_send_html ());
	gtk_signal_connect (GTK_OBJECT (prefs->send_html), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->prompt_empty_subject = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptEmptySubject"));
	gtk_toggle_button_set_active (prefs->prompt_empty_subject, mail_config_get_prompt_empty_subject ());
	gtk_signal_connect (GTK_OBJECT (prefs->prompt_empty_subject), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->prompt_bcc_only = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptBccOnly"));
	gtk_toggle_button_set_active (prefs->prompt_bcc_only, mail_config_get_prompt_only_bcc ());
	gtk_signal_connect (GTK_OBJECT (prefs->prompt_bcc_only), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	menu = e_charset_picker_new (mail_config_get_default_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	option_menu_connect (prefs->charset, prefs);
	
	/* Spell Checking */
	/* FIXME: do stuff with these */
	prefs->spell_check = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkEnableSpellChecking"));
	prefs->colour = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerSpellCheckColor"));
	prefs->language = GTK_COMBO (glade_xml_get_widget (gui, "cmboSpellCheckLanguage"));
	
	/* Forwards and Replies */
	prefs->forward_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuForwardStyle"));
	gtk_option_menu_set_history (prefs->forward_style, mail_config_get_default_forward_style ());
	style = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (prefs->forward_style)),
			       attach_style_info, &style);
	option_menu_connect (prefs->forward_style, prefs);
	
	prefs->reply_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuReplyStyle"));
	gtk_option_menu_set_history (prefs->reply_style, mail_config_get_default_reply_style ());
	style = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (prefs->reply_style)),
			       attach_style_info, &style);
	option_menu_connect (prefs->reply_style, prefs);
	
	/* FIXME: do the other tabs... */
}


GtkWidget *
mail_composer_prefs_new (void)
{
	MailComposerPrefs *new;
	
	new = (MailComposerPrefs *) gtk_type_new (mail_composer_prefs_get_type ());
	mail_composer_prefs_construct (new);
	
	return (GtkWidget *) new;
}


void
mail_composer_prefs_apply (MailComposerPrefs *prefs)
{
	GtkWidget *menu, *item;
	char *string;
	int val;
	
	/* General tab */
	
	/* Default Behavior */
	mail_config_set_send_html (gtk_toggle_button_get_active (prefs->send_html));
	mail_config_set_prompt_empty_subject (gtk_toggle_button_get_active (prefs->prompt_empty_subject));
	mail_config_set_prompt_only_bcc (gtk_toggle_button_get_active (prefs->prompt_bcc_only));
	
	menu = gtk_option_menu_get_menu (prefs->charset);
	string = e_charset_picker_get_charset (menu);
	if (string) {
		mail_config_set_default_charset (string);
		g_free (string);
	}
	
	/* Spell CHecking */
	/* FIXME: implement me */
	
	/* Forwards and Replies */
	menu = gtk_option_menu_get_menu (prefs->forward_style);
	item = gtk_menu_get_active (GTK_MENU (menu));
	val = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "style"));
	mail_config_set_default_forward_style (val);
	
	menu = gtk_option_menu_get_menu (prefs->reply_style);
	item = gtk_menu_get_active (GTK_MENU (menu));
	val = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "style"));
	mail_config_set_default_reply_style (val);
	
	/* Keyboard Shortcuts */
	/* FIXME: implement me */
	
	/* Signatures */
	/* FIXME: implement me */
}
