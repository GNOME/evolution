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


#ifndef __MAIL_COMPOSER_PREFS_H__
#define __MAIL_COMPOSER_PREFS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnomeui/gnome-color-picker.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-file-entry.h>
#include <gtkhtml/gtkhtml.h>

#include "mail-signature-editor.h"

#include "evolution-config-control.h"

#include <shell/Evolution.h>

#define MAIL_COMPOSER_PREFS_TYPE        (mail_composer_prefs_get_type ())
#define MAIL_COMPOSER_PREFS(o)          (GTK_CHECK_CAST ((o), MAIL_COMPOSER_PREFS_TYPE, MailComposerPrefs))
#define MAIL_COMPOSER_PREFS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MAIL_COMPOSER_PREFS_TYPE, MailComposerPrefsClass))
#define IS_MAIL_COMPOSER_PREFS(o)       (GTK_CHECK_TYPE ((o), MAIL_COMPOSER_PREFS_TYPE))
#define IS_MAIL_COMPOSER_PREFS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MAIL_COMPOSER_PREFS_TYPE))

typedef struct _MailComposerPrefs MailComposerPrefs;
typedef struct _MailComposerPrefsClass MailComposerPrefsClass;

struct _MailComposerPrefs {
	GtkVBox parent_object;
	
	EvolutionConfigControl *control;
	
	GladeXML *gui;
	
	/* General tab */
	
	/* Default Behavior */
	GtkToggleButton *send_html;
	GtkToggleButton *prompt_empty_subject;
	GtkToggleButton *prompt_bcc_only;
	GtkOptionMenu *charset;
	
	/* Spell Checking */
	GtkToggleButton *spell_check;
	GnomeColorPicker *colour;
	GtkCombo *language;
	
	/* Forwards and Replies */
	GtkOptionMenu *forward_style;
	GtkOptionMenu *reply_style;
	
	/* Keyboard Shortcuts */
	GtkOptionMenu *shortcuts_type;
	GtkCList *keybindings;
	
	/* Signatures */
	GtkCList *sig_clist;
	GtkButton *sig_add;
	GtkButton *sig_edit;
	GtkButton *sig_delete;
	GtkToggleButton *sig_random;
	GtkToggleButton *sig_html;
	GnomeFileEntry *sig_filename;
	GnomeFileEntry *sig_script;
	GtkHTML *sig_preview;
	gboolean sig_switch;
	int sig_row;
};

struct _MailComposerPrefsClass {
	GtkVBoxClass parent_class;
	
	/* signals */
	
};


GtkType mail_composer_prefs_get_type (void);

GtkWidget *mail_composer_prefs_new (void);

void mail_composer_prefs_apply (MailComposerPrefs *prefs);


MailConfigSignature *mail_composer_prefs_new_signature (MailComposerPrefs *prefs, gboolean html);

/* needed by global config */
#define MAIL_COMPOSER_PREFS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_ComposerPrefs_ConfigControl"

#ifdef __cplusplus
}
#endif

#endif /* __MAIL_COMPOSER_PREFS_H__ */
