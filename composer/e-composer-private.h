/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_COMPOSER_PRIVATE_H
#define E_COMPOSER_PRIVATE_H

#include "e-msg-composer.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include "gconf-bridge.h"

#include <libedataserver/e-iconv.h>

#include "e-attachment-bar.h"
#include "e-composer-actions.h"
#include "e-composer-autosave.h"
#include "e-composer-header-table.h"

#define E_MSG_COMPOSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MSG_COMPOSER, EMsgComposerPrivate))

/* Mail configuration keys */
#define MAIL_GCONF_PREFIX \
	"/apps/evolution/mail"
#define MAIL_GCONF_CHARSET_KEY \
	MAIL_GCONF_PREFIX "/format/charset"

/* Composer configuration keys */
#define COMPOSER_GCONF_PREFIX \
	MAIL_GCONF_PREFIX "/composer"
#define COMPOSER_GCONF_CHARSET_KEY \
	COMPOSER_GCONF_PREFIX "/charset"
#define COMPOSER_GCONF_CURRENT_FOLDER_KEY \
	COMPOSER_GCONF_PREFIX "/current_folder"
#define COMPOSER_GCONF_INLINE_SPELLING_KEY \
	COMPOSER_GCONF_PREFIX "/inline_spelling"
#define COMPOSER_GCONF_MAGIC_LINKS_KEY \
	COMPOSER_GCONF_PREFIX "/magic_links"
#define COMPOSER_GCONF_MAGIC_SMILEYS_KEY \
	COMPOSER_GCONF_PREFIX "/magic_smileys"
#define COMPOSER_GCONF_REQUEST_RECEIPT_KEY \
	COMPOSER_GCONF_PREFIX "/request_receipt"
#define COMPOSER_GCONF_TOP_SIGNATURE_KEY \
	COMPOSER_GCONF_PREFIX "/top_signature"
#define COMPOSER_GCONF_SEND_HTML_KEY \
	COMPOSER_GCONF_PREFIX "/send_html"
#define COMPOSER_GCONF_SPELL_LANGUAGES_KEY \
	COMPOSER_GCONF_PREFIX "/spell_languages"
#define COMPOSER_GCONF_VIEW_BCC_KEY \
	COMPOSER_GCONF_PREFIX "/view/Bcc"
#define COMPOSER_GCONF_VIEW_CC_KEY \
	COMPOSER_GCONF_PREFIX "/view/Cc"
#define COMPOSER_GCONF_VIEW_FROM_KEY \
	COMPOSER_GCONF_PREFIX "/view/From"
#define COMPOSER_GCONF_VIEW_POST_TO_KEY \
	COMPOSER_GCONF_PREFIX "/view/PostTo"
#define COMPOSER_GCONF_VIEW_REPLY_TO_KEY \
	COMPOSER_GCONF_PREFIX "/view/ReplyTo"
#define COMPOSER_GCONF_WINDOW_PREFIX \
	COMPOSER_GCONF_PREFIX "/window"

/* Shorthand, requires a variable named "composer". */
#define ACTION(name)	(E_COMPOSER_ACTION_##name (composer))

/* For use in dispose() methods. */
#define DISPOSE(obj) \
	G_STMT_START { \
	if ((obj) != NULL) { g_object_unref (obj); (obj) = NULL; } \
	} G_STMT_END

G_BEGIN_DECLS

struct _EMsgComposerPrivate {

	/*** UI Management ***/

	GtkWidget *html_editor;
	GtkWidget *header_table;
	GtkActionGroup *charset_actions;
	GtkActionGroup *composer_actions;

	GPtrArray *extra_hdr_names, *extra_hdr_values;
	GArray *gconf_bridge_binding_ids;

	GtkWidget *focused_entry;

	GtkWidget *attachment_bar;
	GtkWidget *attachment_scrolled_window;
	GtkWidget *attachment_expander;
	GtkWidget *attachment_expander_label;
	GtkWidget *attachment_expander_icon;
	GtkWidget *attachment_expander_num;

	GtkWidget *address_dialog;

	GHashTable *inline_images;
	GHashTable *inline_images_by_url;
	GList *current_images;

	gchar *mime_type, *mime_body, *charset;

	guint32 attachment_bar_visible : 1;
	guint32 is_alternative         : 1;
	guint32 autosaved              : 1;

	guint32 mode_post              : 1;

	guint32 in_signature_insert    : 1;

	CamelMimeMessage *redirect;

	guint notify_id;

	gboolean send_invoked;
};

void		e_composer_private_init		(EMsgComposer *composer);
void		e_composer_private_dispose	(EMsgComposer *composer);
void		e_composer_private_finalize	(EMsgComposer *composer);

/* Private Utilities */

void		e_composer_actions_init		(EMsgComposer *composer);
gchar *		e_composer_find_data_file	(const gchar *basename);
gchar *		e_composer_get_default_charset	(void);

G_END_DECLS

#endif /* E_COMPOSER_PRIVATE_H */
