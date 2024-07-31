/*
 * e-mail-reader.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_READER_H
#define E_MAIL_READER_H

/* XXX Anjal uses a different message list widget than Evolution, so
 *     avoid including <mail/message-list.h> in this file.  This makes
 *     the get_message_list() method a little awkward since it returns
 *     a GtkWidget pointer which almost always has to be type casted. */

#include <gtk/gtk.h>
#include <camel/camel.h>
#include <e-util/e-util.h>
#include <composer/e-msg-composer.h>

#include <mail/e-mail-backend.h>
#include <mail/e-mail-display.h>
#include <mail/e-mail-enums.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_READER \
	(e_mail_reader_get_type ())
#define E_MAIL_READER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_READER, EMailReader))
#define E_MAIL_READER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_READER, EMailReaderInterface))
#define E_IS_MAIL_READER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_READER))
#define E_IS_MAIL_READER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_READER))
#define E_MAIL_READER_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_MAIL_READER, EMailReaderInterface))

G_BEGIN_DECLS

typedef struct _EMailReader EMailReader;
typedef struct _EMailReaderInterface EMailReaderInterface;

typedef enum {
	E_MAIL_READER_ACTION_GROUP_STANDARD,
	E_MAIL_READER_ACTION_GROUP_SEARCH_FOLDERS,
	E_MAIL_READER_ACTION_GROUP_LABELS,
	E_MAIL_READER_NUM_ACTION_GROUPS
} EMailReaderActionGroup;

enum {
	E_MAIL_READER_HAVE_ENABLED_ACCOUNT = 1 << 0,
	E_MAIL_READER_SELECTION_SINGLE = 1 << 1,
	E_MAIL_READER_SELECTION_MULTIPLE = 1 << 2,
	E_MAIL_READER_SELECTION_CAN_ADD_SENDER = 1 << 3,
	E_MAIL_READER_SELECTION_FLAG_CLEAR = 1 << 4,
	E_MAIL_READER_SELECTION_FLAG_COMPLETED = 1 << 5,
	E_MAIL_READER_SELECTION_FLAG_FOLLOWUP = 1 << 6,
	E_MAIL_READER_SELECTION_HAS_DELETED = 1 << 7,
	E_MAIL_READER_SELECTION_HAS_IMPORTANT = 1 << 8,
	E_MAIL_READER_SELECTION_HAS_JUNK = 1 << 9,
	E_MAIL_READER_SELECTION_HAS_NOT_JUNK = 1 << 10,
	E_MAIL_READER_SELECTION_HAS_READ = 1 << 11,
	E_MAIL_READER_SELECTION_HAS_UNDELETED = 1 << 12,
	E_MAIL_READER_SELECTION_HAS_UNIMPORTANT = 1 << 13,
	E_MAIL_READER_SELECTION_HAS_UNREAD = 1 << 14,
	E_MAIL_READER_SELECTION_HAS_ATTACHMENTS = 1 << 15,
	E_MAIL_READER_SELECTION_IS_MAILING_LIST = 1 << 16,
	E_MAIL_READER_FOLDER_IS_JUNK = 1 << 17,
	E_MAIL_READER_FOLDER_IS_VTRASH = 1 << 18,
	E_MAIL_READER_SELECTION_HAS_IGNORE_THREAD = 1 << 20,
	E_MAIL_READER_SELECTION_HAS_NOTIGNORE_THREAD = 1 << 21,
	E_MAIL_READER_SELECTION_HAS_MAIL_NOTE = 1 << 22,
	E_MAIL_READER_SELECTION_HAS_COLOR = 1 << 23
};

struct _EMailReaderInterface {
	GTypeInterface parent_interface;

	void		(*init_ui_data)		(EMailReader *reader);
	EUIManager *	(*get_ui_manager)	(EMailReader *reader);
	EAlertSink *	(*get_alert_sink)	(EMailReader *reader);
	EMailBackend *	(*get_backend)		(EMailReader *reader);
	EMailDisplay *	(*get_mail_display)	(EMailReader *reader);
	gboolean	(*get_hide_deleted)	(EMailReader *reader);
	GtkWidget *	(*get_message_list)	(EMailReader *reader);
	EPreviewPane *	(*get_preview_pane)	(EMailReader *reader);
	GPtrArray *	(*get_selected_uids)	(EMailReader *reader);
	GPtrArray *	(*get_selected_uids_with_collapsed_threads)
						(EMailReader *reader);
	GtkWindow *	(*get_window)		(EMailReader *reader);

	CamelFolder *	(*ref_folder)		(EMailReader *reader);
	void		(*set_folder)		(EMailReader *reader,
						 CamelFolder *folder);
	void		(*set_message)		(EMailReader *reader,
						 const gchar *message_uid);
	guint		(*open_selected_mail)	(EMailReader *reader);

	/* Signals */
	void		(*composer_created)	(EMailReader *reader,
						 EMsgComposer *composer,
						 CamelMimeMessage *source);
	void		(*folder_loaded)	(EMailReader *reader);
	void		(*message_loaded)	(EMailReader *reader,
						 const gchar *message_uid,
						 CamelMimeMessage *message);
	void		(*message_seen)		(EMailReader *reader,
						 const gchar *message_uid,
						 CamelMimeMessage *message);
	void		(*show_search_bar)	(EMailReader *reader);
	void		(*update_actions)	(EMailReader *reader,
						 guint32 state);
	gboolean	(*close_on_delete_or_junk)
						(EMailReader *reader);
	void		(*reload)		(EMailReader *reader);

	/* Padding for future expansion */
	gpointer reserved[1];
};

GType		e_mail_reader_get_type		(void);
void		e_mail_reader_init		(EMailReader *reader);
void		e_mail_reader_dispose		(EMailReader *reader);
void		e_mail_reader_changed		(EMailReader *reader);
guint32		e_mail_reader_check_state	(EMailReader *reader);
EActivity *	e_mail_reader_new_activity	(EMailReader *reader);
void		e_mail_reader_update_actions	(EMailReader *reader,
						 guint32 state);
void		e_mail_reader_init_ui_data_default
						(EMailReader *self);
void		e_mail_reader_init_ui_data	(EMailReader *reader);
EUIManager *	e_mail_reader_get_ui_manager	(EMailReader *reader);
EUIAction *	e_mail_reader_get_action	(EMailReader *reader,
						 const gchar *action_name);
EAlertSink *	e_mail_reader_get_alert_sink	(EMailReader *reader);
EMailBackend *	e_mail_reader_get_backend	(EMailReader *reader);
EMailDisplay *	e_mail_reader_get_mail_display	(EMailReader *reader);
gboolean	e_mail_reader_get_hide_deleted	(EMailReader *reader);
GtkWidget *	e_mail_reader_get_message_list	(EMailReader *reader);
guint		e_mail_reader_open_selected_mail
						(EMailReader *reader);
GtkMenu *	e_mail_reader_get_popup_menu	(EMailReader *reader);
EPreviewPane *	e_mail_reader_get_preview_pane	(EMailReader *reader);
GPtrArray *	e_mail_reader_get_selected_uids	(EMailReader *reader);
GPtrArray *	e_mail_reader_get_selected_uids_with_collapsed_threads
						(EMailReader *reader);
GtkWindow *	e_mail_reader_get_window	(EMailReader *reader);
gboolean	e_mail_reader_close_on_delete_or_junk
						(EMailReader *reader);
CamelFolder *	e_mail_reader_ref_folder	(EMailReader *reader);
void		e_mail_reader_set_folder	(EMailReader *reader,
						 CamelFolder *folder);
void		e_mail_reader_set_message	(EMailReader *reader,
						 const gchar *message_uid);
EMailForwardStyle
		e_mail_reader_get_forward_style	(EMailReader *reader);
void		e_mail_reader_set_forward_style	(EMailReader *reader,
						 EMailForwardStyle style);
gboolean	e_mail_reader_get_group_by_threads
						(EMailReader *reader);
void		e_mail_reader_set_group_by_threads
						(EMailReader *reader,
						 gboolean group_by_threads);
EMailReplyStyle	e_mail_reader_get_reply_style	(EMailReader *reader);
void		e_mail_reader_set_reply_style	(EMailReader *reader,
						 EMailReplyStyle style);
gboolean	e_mail_reader_get_mark_seen_always
						(EMailReader *reader);
void		e_mail_reader_set_mark_seen_always
						(EMailReader *reader,
						 gboolean mark_seen_always);
gboolean	e_mail_reader_get_delete_selects_previous
						(EMailReader *reader);
void		e_mail_reader_set_delete_selects_previous
						(EMailReader *reader,
						 gboolean delete_selects_previous);
void		e_mail_reader_show_search_bar	(EMailReader *reader);
void		e_mail_reader_avoid_next_mark_as_seen
						(EMailReader *reader);
void		e_mail_reader_unset_folder_just_selected
						(EMailReader *reader);
void		e_mail_reader_composer_created	(EMailReader *reader,
						 EMsgComposer *composer,
						 CamelMimeMessage *message);
void		e_mail_reader_reload		(EMailReader *reader);
gboolean	e_mail_reader_ignore_accel	(EMailReader *reader);

G_END_DECLS

#endif /* E_MAIL_READER_H */
