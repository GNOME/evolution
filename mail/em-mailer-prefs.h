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


#ifndef __EM_MAILER_PREFS_H__
#define __EM_MAILER_PREFS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-file-entry.h>
#include <libgnomeui/gnome-color-picker.h>
#include <libgnomeui/gnome-font-picker.h>

#include "evolution-config-control.h"
#include "em-format.h"

#include <shell/Evolution.h>

#define EM_MAILER_PREFS_TYPE        (em_mailer_prefs_get_type ())
#define EM_MAILER_PREFS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EM_MAILER_PREFS_TYPE, EMMailerPrefs))
#define EM_MAILER_PREFS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EM_MAILER_PREFS_TYPE, EMMailerPrefsClass))
#define EM_IS_MAILER_PREFS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EM_MAILER_PREFS_TYPE))
#define EM_IS_MAILER_PREFS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EM_MAILER_PREFS_TYPE))

typedef struct _EMMailerPrefs EMMailerPrefs;
typedef struct _EMMailerPrefsClass EMMailerPrefsClass;
typedef struct _EMMailerPrefsHeader EMMailerPrefsHeader;

struct _EMMailerPrefsHeader {
	char *name;
	int enabled:1;
	int is_default:1;
};

struct _EMMailerPrefs {
	GtkVBox parent_object;
	
	GNOME_Evolution_Shell shell;
	
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
	GtkOptionMenu *empty_trash_days;
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

	/* Headers tab */
	GtkButton *add_header;
	GtkButton *remove_header;
	GtkEntry *entry_header;
	GtkTreeView *header_list;
	GtkListStore *header_list_store;

	/* Junk prefs */
	GtkToggleButton *check_incoming;
	GtkToggleButton *sa_local_tests_only;
	GtkToggleButton *sa_use_daemon;
};

struct _EMMailerPrefsClass {
	GtkVBoxClass parent_class;
	
	/* signals */
	
};


GtkType em_mailer_prefs_get_type (void);

GtkWidget *em_mailer_prefs_new (void);

EMMailerPrefsHeader *em_mailer_prefs_header_from_xml(const char *xml);
char *em_mailer_prefs_header_to_xml(EMMailerPrefsHeader *header);
void em_mailer_prefs_header_free(EMMailerPrefsHeader *header);

/* needed by global config */
#define EM_MAILER_PREFS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_MailerPrefs_ConfigControl:" BASE_VERSION

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_MAILER_PREFS_H__ */
