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
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MSG_COMPOSER_H
#define E_MSG_COMPOSER_H

#include <camel/camel.h>
#include <libebook/libebook.h>

#include <shell/e-shell.h>

#include <composer/e-composer-header-table.h>
#include <em-format/e-mail-part-list.h>

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
	GtkWindow parent;
	EMsgComposerPrivate *priv;
};

struct _EMsgComposerClass {
	GtkWindowClass parent_class;

	/* Signals */
	gboolean	(*presend)		(EMsgComposer *composer);
	void		(*print)		(EMsgComposer *composer,
						 GtkPrintOperationAction print_action,
						 CamelMimeMessage *message,
						 EActivity *activity);
	void		(*save_to_drafts)	(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 EActivity *activity);
	void		(*save_to_outbox)	(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 EActivity *activity);
	void		(*send)			(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 EActivity *activity);
};

GType		e_msg_composer_get_type		(void);
void		e_msg_composer_new		(EShell *shell,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
EMsgComposer *	e_msg_composer_new_finish	(GAsyncResult *result,
						 GError **error);
void		e_msg_composer_setup_with_message
						(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 gboolean keep_signature,
						 const gchar *override_identity_uid,
						 const gchar *override_alias_name,
						 const gchar *override_alias_address,
						 GCancellable *cancellable);
void		e_msg_composer_setup_from_url	(EMsgComposer *composer,
						 const gchar *url);
void		e_msg_composer_setup_redirect	(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 const gchar *identity_uid,
						 const gchar *alias_name,
						 const gchar *alias_address,
						 GCancellable *cancellable);
EHTMLEditor *	e_msg_composer_get_editor	(EMsgComposer *composer);
EFocusTracker *	e_msg_composer_get_focus_tracker
						(EMsgComposer *composer);
CamelSession *	e_msg_composer_ref_session	(EMsgComposer *composer);
EShell *	e_msg_composer_get_shell	(EMsgComposer *composer);

EContentEditorContentHash *
		e_msg_composer_get_content_hash	(EMsgComposer *composer);
void		e_msg_composer_send		(EMsgComposer *composer);
void		e_msg_composer_save_to_drafts	(EMsgComposer *composer);
void		e_msg_composer_save_to_outbox	(EMsgComposer *composer);
void		e_msg_composer_print		(EMsgComposer *composer,
						 GtkPrintOperationAction print_action);

void		e_msg_composer_set_body_text	(EMsgComposer *composer,
						 const gchar *text,
						 gboolean update_signature);
void		e_msg_composer_set_body		(EMsgComposer *composer,
						 const gchar *body,
						 const gchar *mime_type);
void		e_msg_composer_add_header	(EMsgComposer *composer,
						 const gchar *name,
						 const gchar *value);
void		e_msg_composer_set_header	(EMsgComposer *composer,
						 const gchar *name,
						 const gchar *value);
const gchar *	e_msg_composer_get_header	(EMsgComposer *composer,
						 const gchar *name,
						 gint index);
void		e_msg_composer_remove_header	(EMsgComposer *composer,
						 const gchar *name);
void		e_msg_composer_set_draft_headers
						(EMsgComposer *composer,
						 const gchar *folder_uri,
						 const gchar *message_uid);
void		e_msg_composer_set_source_headers
						(EMsgComposer *composer,
						 const gchar *folder_uri,
						 const gchar *message_uid,
						 CamelMessageFlags flags);
void		e_msg_composer_attach		(EMsgComposer *composer,
						 CamelMimePart *mime_part);
void		e_msg_composer_set_alternative_body
						(EMsgComposer *composer,
						 CamelMimePart *mime_part);
void		e_msg_composer_get_message	(EMsgComposer *composer,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelMimeMessage *
		e_msg_composer_get_message_finish
						(EMsgComposer *composer,
						 GAsyncResult *result,
						 GError **error);
void		e_msg_composer_get_message_print
						(EMsgComposer *composer,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelMimeMessage *
		e_msg_composer_get_message_print_finish
						(EMsgComposer *composer,
						 GAsyncResult *result,
						 GError **error);
void		e_msg_composer_get_message_draft
						(EMsgComposer *composer,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelMimeMessage *
		e_msg_composer_get_message_draft_finish
						(EMsgComposer *composer,
						 GAsyncResult *result,
						 GError **error);

CamelInternetAddress *
		e_msg_composer_get_from		(EMsgComposer *composer);
CamelInternetAddress *
		e_msg_composer_get_reply_to	(EMsgComposer *composer);

void		e_msg_composer_add_message_attachments
						(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 gboolean just_inlines);
void		e_msg_composer_add_attachments_from_part_list
						(EMsgComposer *composer,
						 EMailPartList *part_list,
						 gboolean just_inlines);
void		e_msg_composer_check_inline_attachments
						(EMsgComposer *composer);

void		e_msg_composer_request_close	(EMsgComposer *composer);
gboolean	e_msg_composer_can_close	(EMsgComposer *composer,
						 gboolean can_save_draft);

EComposerHeaderTable *
		e_msg_composer_get_header_table	(EMsgComposer *composer);
EAttachmentView *
		e_msg_composer_get_attachment_view
						(EMsgComposer *composer);
GByteArray *	e_msg_composer_get_raw_message_text_without_signature
						(EMsgComposer *composer);
GByteArray *	e_msg_composer_get_raw_message_text
						(EMsgComposer *composer);

gboolean	e_msg_composer_is_exiting	(EMsgComposer *composer);

void		e_save_spell_languages		(const GList *spell_languages);
void		e_msg_composer_save_focused_widget
						(EMsgComposer *composer);
void		e_msg_composer_restore_focus_on_composer
						(EMsgComposer *composer);
gboolean	e_msg_composer_is_busy		(EMsgComposer *composer);
gboolean	e_msg_composer_is_soft_busy	(EMsgComposer *composer);
gboolean	e_msg_composer_get_is_reply_or_forward
						(EMsgComposer *composer);
void		e_msg_composer_set_is_reply_or_forward
						(EMsgComposer *composer,
						 gboolean is_reply_or_forward);
void		e_msg_composer_check_autocrypt	(EMsgComposer *composer,
						 CamelMimeMessage *original_message);
void		e_msg_composer_set_is_imip	(EMsgComposer *composer,
						 gboolean is_imip);
gboolean	e_msg_composer_get_is_imip	(EMsgComposer *composer);

G_END_DECLS

#endif /* E_MSG_COMPOSER_H */
