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


#ifndef __MAIL_PREFERENCES_H__
#define __MAIL_PREFERENCES_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-file-entry.h>
#include <libgnomeui/gnome-color-picker.h>
#include <libgnomeui/gnome-font-picker.h>

#include "evolution-config-control.h"

#include <shell/Evolution.h>

#define MAIL_PREFERENCES_TYPE        (mail_preferences_get_type ())
#define MAIL_PREFERENCES(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIL_PREFERENCES_TYPE, MailPreferences))
#define MAIL_PREFERENCES_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), MAIL_PREFERENCES_TYPE, MailPreferencesClass))
#define IS_MAIL_PREFERENCES(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIL_PREFERENCES_TYPE))
#define IS_MAIL_PREFERENCES_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MAIL_PREFERENCES_TYPE))

typedef struct _MailPreferences MailPreferences;
typedef struct _MailPreferencesClass MailPreferencesClass;

struct _MailPreferences {
	GtkVBox parent_object;
	
	GNOME_Evolution_Shell shell;
	
	EvolutionConfigControl *control;
	
	GladeXML *gui;
	GConfClient *gconf;
	
	/* General tab */
	
	/* Message Display */
	GtkToggleButton *timeout_toggle;
	GtkSpinButton *timeout;
	GtkOptionMenu *charset;
	GtkToggleButton *citation_highlight;
	GnomeColorPicker *citation_color;
	
	/* Deleting Mail */
	GtkToggleButton *empty_trash;
	GtkToggleButton *confirm_expunge;
	
	/* New Mail Notification */
	GtkToggleButton *notify_not;
	GtkToggleButton *notify_beep;
	GtkToggleButton *notify_play_sound;
	GnomeFileEntry *notify_sound_file;
	
	/* HTML Mail tab */
	GnomeFontPicker *font_variable;
	GnomeFontPicker *font_fixed;
	GtkToggleButton *font_share;
	
	/* Loading Images */
	GtkToggleButton *images_always;
	GtkToggleButton *images_sometimes;
	GtkToggleButton *images_never;
	
	GtkToggleButton *show_animated;
	GtkToggleButton *autodetect_links;
	GtkToggleButton *prompt_unwanted_html;

	/* Labels and Colours tab */
	struct {
		GtkEntry *name;
		GnomeColorPicker *color;
	} labels[5];
	GtkButton *restore_labels;
};

struct _MailPreferencesClass {
	GtkVBoxClass parent_class;
	
	/* signals */
	
};


GtkType mail_preferences_get_type (void);

GtkWidget *mail_preferences_new (void);

void mail_preferences_apply (MailPreferences *prefs);

/* needed by global config */
#define MAIL_PREFERENCES_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_Preferences_ConfigControl"

#ifdef __cplusplus
}
#endif

#endif /* __MAIL_PREFERENCES_H__ */
