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
#include "e-composer-activity.h"
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

	/*** UI Management ***/

	GtkWidget *header_table;
	GtkWidget *activity_bar;
	GtkWidget *alert_bar;
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

	GHashTable *inline_images;
	GHashTable *inline_images_by_url;
	GList *current_images;

	gchar *mime_type;
	gchar *mime_body;
	gchar *charset;

	guint32 autosaved : 1;
	guint32 mode_post : 1;
	guint32 in_signature_insert : 1;
	guint32 application_exiting : 1;

	CamelMimeMessage *redirect;

	gboolean is_from_message;
	gboolean disable_signature;

	gchar *selected_signature_uid;
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
gboolean	e_composer_paste_html		(EMsgComposer *composer,
						 GtkClipboard *clipboard);
gboolean	e_composer_paste_image		(EMsgComposer *composer,
						 GtkClipboard *clipboard);
gboolean	e_composer_paste_text		(EMsgComposer *composer,
						 GtkClipboard *clipboard);
gboolean	e_composer_paste_uris		(EMsgComposer *composer,
						 GtkClipboard *clipboard);
gboolean	e_composer_selection_is_image_uris
						(EMsgComposer *composer,
						 GtkSelectionData *selection);
void		e_composer_update_signature	(EMsgComposer *composer);

G_END_DECLS

#endif /* E_COMPOSER_PRIVATE_H */
