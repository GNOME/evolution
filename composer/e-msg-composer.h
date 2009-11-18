/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MSG_COMPOSER_H
#define E_MSG_COMPOSER_H

#include <camel/camel-internet-address.h>
#include <camel/camel-mime-message.h>
#include <libedataserver/e-account.h>
#include <libebook/e-destination.h>
#include <gtkhtml-editor.h>
#include <misc/e-attachment-view.h>

#include "e-composer-header-table.h"

/* Standard GObject macros */
#define E_TYPE_MSG_COMPOSER \
	(e_msg_composer_get_type ())
#define E_MSG_COMPOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MSG_COMPOSER, EMsgComposer))
#define E_MSG_COMPOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MSG_COMPOSER, EMsgComposerClass))
#define E_IS_MSG_COMPOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MSG_COMPOSER))
#define E_IS_MSG_COMPOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_MSG_COMPOSER))
#define E_MSG_COMPOSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MSG_COMPOSER, EMsgComposerClass))

G_BEGIN_DECLS

typedef struct _EMsgComposer EMsgComposer;
typedef struct _EMsgComposerClass EMsgComposerClass;
typedef struct _EMsgComposerPrivate EMsgComposerPrivate;

struct _EMsgComposer {
	GtkhtmlEditor parent;
	EMsgComposerPrivate *priv;
	gboolean lite;
};

struct _EMsgComposerClass {
	GtkhtmlEditorClass parent_class;
};

struct _EAttachmentBar;

GType		e_msg_composer_get_type		(void);
EMsgComposer *	e_msg_composer_new		(void);
EMsgComposer *  e_msg_composer_lite_new	(void);

EMsgComposer *	e_msg_composer_new_with_message	(CamelMimeMessage *msg);
EMsgComposer *	e_msg_composer_new_from_url	(const gchar *url);
EMsgComposer *	e_msg_composer_new_redirect	(CamelMimeMessage *message,
						 const gchar *resent_from);
void		e_msg_composer_set_lite	(void);
gboolean	e_msg_composer_get_lite	(void);

void		e_msg_composer_send		(EMsgComposer *composer);
void		e_msg_composer_save_draft	(EMsgComposer *composer);

void		e_msg_composer_set_alternative	(EMsgComposer *composer,
						 gboolean alt);

void		e_msg_composer_set_body_text	(EMsgComposer *composer,
						 const gchar *text,
						 gssize len);
void		e_msg_composer_set_body		(EMsgComposer *composer,
						 const gchar *body,
						 const gchar *mime_type);
void		e_msg_composer_add_header	(EMsgComposer *composer,
						 const gchar *name,
						 const gchar *value);
void		e_msg_composer_modify_header	(EMsgComposer *composer,
						 const gchar *name,
						 const gchar *value);
void		e_msg_composer_remove_header	(EMsgComposer *composer,
						 const gchar *name);
void		e_msg_composer_attach		(EMsgComposer *composer,
						 CamelMimePart *mime_part);
CamelMimePart *	e_msg_composer_add_inline_image_from_file
						(EMsgComposer *composer,
						 const gchar *filename);
void		e_msg_composer_add_inline_image_from_mime_part
						(EMsgComposer *composer,
						 CamelMimePart *part);
CamelMimeMessage *
		e_msg_composer_get_message	(EMsgComposer *composer,
						 gboolean save_html_object_data);
CamelMimeMessage *
		e_msg_composer_get_message_print(EMsgComposer *composer,
						 gboolean save_html_object_data);
CamelMimeMessage *
		e_msg_composer_get_message_draft(EMsgComposer *composer);
void		e_msg_composer_show_sig_file	(EMsgComposer *composer);

CamelInternetAddress *
		e_msg_composer_get_from		(EMsgComposer *composer);
CamelInternetAddress *
		e_msg_composer_get_reply_to	(EMsgComposer *composer);

void		e_msg_composer_clear_inlined_table
						(EMsgComposer *composer);
void		e_msg_composer_set_enable_autosave
						(EMsgComposer *composer,
						 gboolean enabled);

gchar *		e_msg_composer_get_sig_file_content
						(const gchar *sigfile,
						 gboolean in_html);
void		e_msg_composer_add_message_attachments
						(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 gboolean just_inlines);

void		e_msg_composer_request_close	(EMsgComposer *composer);
void		e_msg_composer_request_close_all(void);
void		e_msg_composer_close		(EMsgComposer *composer);
gboolean	e_msg_composer_all_closed (void);
EMsgComposer *	e_msg_composer_load_from_file	(const gchar *filename);
void		e_msg_composer_check_autosave	(GtkWindow *parent);

void		e_msg_composer_reply_indent	(EMsgComposer *composer);

EComposerHeaderTable *
		e_msg_composer_get_header_table	(EMsgComposer *composer);
EAttachmentView *
		e_msg_composer_get_attachment_view
						(EMsgComposer *composer);
void		e_msg_composer_set_send_options	(EMsgComposer *composer,
						 gboolean send_enable);
GByteArray *	e_msg_composer_get_raw_message_text
						(EMsgComposer *composer);

gboolean	e_msg_composer_is_exiting	(EMsgComposer *composer);

GList *		e_load_spell_languages		(void);
void		e_save_spell_languages		(GList *spell_languages);

void		e_msg_composer_set_mail_sent 	(EMsgComposer *composer, gboolean mail_sent);
gboolean	e_msg_composer_get_mail_sent 	(EMsgComposer *composer);

G_END_DECLS

#endif /* E_MSG_COMPOSER_H */
