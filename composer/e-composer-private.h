/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef E_COMPOSER_PRIVATE_H
#define E_COMPOSER_PRIVATE_H

#include "e-msg-composer.h"

#include <errno.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include "e-composer-actions.h"
#include "e-composer-header-table.h"

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#define E_MSG_COMPOSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MSG_COMPOSER, EMsgComposerPrivate))

/* Shorthand, requires a variable named "composer". */
#define ACTION(name)	(E_COMPOSER_ACTION_##name (composer))

/* For use in dispose() methods. */
#define DISPOSE(obj) \
	G_STMT_START { \
	if ((obj) != NULL) { g_object_unref (obj); (obj) = NULL; } \
	} G_STMT_END

G_BEGIN_DECLS

struct _EMsgComposerPrivate {

	gpointer shell;  /* weak pointer */

	EHTMLEditor *editor;

	/*** UI Management ***/

	GtkWidget *header_table;
	GtkWidget *attachment_paned;

	EFocusTracker *focus_tracker;
	GtkWindowGroup *window_group;

	GtkActionGroup *async_actions;
	GtkActionGroup *charset_actions;
	GtkActionGroup *composer_actions;

	GPtrArray *extra_hdr_names;
	GPtrArray *extra_hdr_values;

	GtkWidget *focused_entry;

	GtkWidget *gallery_icon_view;
	GtkWidget *gallery_scrolled_window;

	GtkWidget *address_dialog;

	gchar *mime_type;
	gchar *mime_body;
	gchar *charset;

	guint32 autosaved : 1;
	guint32 mode_post : 1;
	guint32 in_signature_insert : 1;
	guint32 application_exiting : 1;

	CamelMimeMessage *redirect;

	gboolean busy;
	gboolean disable_signature;
	gboolean is_from_draft;
	gboolean is_from_new_message;
	/* The web view is uneditable while the editor is busy.
	 * This is used to restore the previous editable state. */
	gboolean saved_editable;
	gboolean set_signature_from_message;
	gboolean drop_occured;
	gboolean dnd_is_uri;
	gboolean is_sending_message;
	gboolean dnd_history_saved;
	gboolean check_if_signature_is_changed;
	gboolean ignore_next_signature_change;
	gboolean last_signal_was_paste_primary;

	gint focused_entry_selection_start;
	gint focused_entry_selection_end;

	gulong notify_destinations_bcc_handler;
	gulong notify_destinations_cc_handler;
	gulong notify_destinations_to_handler;
	gulong notify_identity_uid_handler;
	gulong notify_reply_to_handler;
	gulong notify_signature_uid_handler;
	gulong notify_subject_handler;
	gulong notify_subject_changed_handler;
};

void		e_composer_private_constructed	(EMsgComposer *composer);
void		e_composer_private_dispose	(EMsgComposer *composer);
void		e_composer_private_finalize	(EMsgComposer *composer);

/* Private Utilities */

void		e_composer_actions_init		(EMsgComposer *composer);
gchar *		e_composer_find_data_file	(const gchar *basename);
gchar *		e_composer_get_default_charset	(void);
gchar *		e_composer_decode_clue_value	(const gchar *encoded_value);
gchar *		e_composer_encode_clue_value	(const gchar *decoded_value);
gboolean	e_composer_paste_image		(EMsgComposer *composer,
						 GtkClipboard *clipboard);
gboolean	e_composer_paste_uris		(EMsgComposer *composer,
						 GtkClipboard *clipboard);
gboolean	e_composer_selection_is_base64_uris
						(EMsgComposer *composer,
						 GtkSelectionData *selection);
gboolean	e_composer_selection_is_image_uris
						(EMsgComposer *composer,
						 GtkSelectionData *selection);
void		e_composer_update_signature	(EMsgComposer *composer);

G_END_DECLS

#endif /* E_COMPOSER_PRIVATE_H */
