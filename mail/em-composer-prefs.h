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


#ifndef __EM_COMPOSER_PREFS_H__
#define __EM_COMPOSER_PREFS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <gtk/gtkvbox.h>
#include "Spell.h"

#define EM_COMPOSER_PREFS_TYPE        (em_composer_prefs_get_type ())
#define EM_COMPOSER_PREFS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EM_COMPOSER_PREFS_TYPE, EMComposerPrefs))
#define EM_COMPOSER_PREFS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EM_COMPOSER_PREFS_TYPE, EMComposerPrefsClass))
#define EM_IS_COMPOSER_PREFS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EM_COMPOSER_PREFS_TYPE))
#define EM_IS_COMPOSER_PREFS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EM_COMPOSER_PREFS_TYPE))

typedef struct _EMComposerPrefs EMComposerPrefs;
typedef struct _EMComposerPrefsClass EMComposerPrefsClass;

struct _ESignature;
struct _GtkToggleButton;
struct _GtkOptionMenu;
struct _GdkPixbuf;
struct _GtkWidget;
struct _GladeXML;
struct _GnomeColorPicker;
struct _GConfClient;
struct _GtkButton;
struct _GtkTreeView;
struct _GtkWindow;

struct _EMComposerPrefs {
	GtkVBox parent_object;
	
	struct _GConfClient *gconf;
	
	struct _GladeXML *gui;
	
	/* General tab */
	
	/* Default Behavior */
	struct _GtkToggleButton *send_html;
	struct _GtkToggleButton *auto_smileys;
	struct _GtkToggleButton *prompt_empty_subject;
	struct _GtkToggleButton *prompt_bcc_only;
	struct _GtkOptionMenu *charset;
	
	struct _GtkToggleButton *spell_check;
	struct _GnomeColorPicker *colour;
	struct _GtkTreeView *language;
	CORBA_sequence_GNOME_Spell_Language *language_seq;
	gboolean spell_active;
	
	struct _GdkPixbuf *enabled_pixbuf;
	struct _GtkWidget *spell_able_button;
	
	/* Forwards and Replies */
	struct _GtkOptionMenu *forward_style;
	struct _GtkOptionMenu *reply_style;
	
	/* Keyboard Shortcuts */
	struct _GtkOptionMenu *shortcuts_type;
	
	/* Signatures */
	struct _GtkTreeView *sig_list;
	GHashTable *sig_hash;
	struct _GtkButton *sig_add;
	struct _GtkButton *sig_add_script;
	struct _GtkButton *sig_edit;
	struct _GtkButton *sig_delete;
	struct _GtkHTML *sig_preview;
	
	struct _GladeXML *sig_script_gui;
	struct _GtkWidget *sig_script_dialog;
	
	guint sig_added_id;
	guint sig_removed_id;
	guint sig_changed_id;
};

struct _EMComposerPrefsClass {
	GtkVBoxClass parent_class;
	
	/* signals */
	
};

GType em_composer_prefs_get_type (void);

struct _GtkWidget *em_composer_prefs_new (void);

void em_composer_prefs_new_signature (struct _GtkWindow *parent, gboolean html);

/* needed by global config */
#define EM_COMPOSER_PREFS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_ComposerPrefs_ConfigControl:" BASE_VERSION

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_COMPOSER_PREFS_H__ */
