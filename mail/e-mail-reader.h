/*
 * e-mail-reader.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_READER_H
#define E_MAIL_READER_H

#include <gtk/gtk.h>
#include <camel/camel-folder.h>
#include <mail/em-format-html-display.h>
#include <mail/message-list.h>
#include <shell/e-shell-module.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_READER \
	(e_mail_reader_get_type ())
#define E_MAIL_READER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_READER, EMailReader))
#define E_IS_MAIL_READER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_READER))
#define E_MAIL_READER_GET_IFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_MAIL_READER, EMailReaderIface))

/* Basename of the UI definition file. */
#define E_MAIL_READER_UI_DEFINITION	"evolution-mail-reader.ui"

G_BEGIN_DECLS

typedef struct _EMailReader EMailReader;
typedef struct _EMailReaderIface EMailReaderIface;

enum {
	E_MAIL_READER_SELECTION_SINGLE			= 1 << 0,
	E_MAIL_READER_SELECTION_MULTIPLE		= 1 << 1,
	E_MAIL_READER_SELECTION_CAN_ADD_SENDER		= 1 << 2,
	E_MAIL_READER_SELECTION_CAN_EDIT		= 1 << 3,
	E_MAIL_READER_SELECTION_FLAG_CLEAR		= 1 << 4,
	E_MAIL_READER_SELECTION_FLAG_COMPLETED		= 1 << 5,
	E_MAIL_READER_SELECTION_FLAG_FOLLOWUP		= 1 << 6,
	E_MAIL_READER_SELECTION_HAS_DELETED		= 1 << 7,
	E_MAIL_READER_SELECTION_HAS_IMPORTANT		= 1 << 8,
	E_MAIL_READER_SELECTION_HAS_JUNK		= 1 << 9,
	E_MAIL_READER_SELECTION_HAS_NOT_JUNK		= 1 << 10,
	E_MAIL_READER_SELECTION_HAS_READ		= 1 << 11,
	E_MAIL_READER_SELECTION_HAS_UNDELETED		= 1 << 12,
	E_MAIL_READER_SELECTION_HAS_UNIMPORTANT		= 1 << 13,
	E_MAIL_READER_SELECTION_HAS_UNREAD		= 1 << 14,
	E_MAIL_READER_SELECTION_HAS_URI_CALLTO		= 1 << 15,
	E_MAIL_READER_SELECTION_HAS_URI_HTTP		= 1 << 16,
	E_MAIL_READER_SELECTION_HAS_URI_MAILTO		= 1 << 17,
	E_MAIL_READER_SELECTION_IS_MAILING_LIST		= 1 << 18
};

struct _EMailReaderIface {
	GTypeInterface parent_iface;

	GtkActionGroup *
			(*get_action_group)	(EMailReader *reader);
	gboolean	(*get_hide_deleted)	(EMailReader *reader);
	EMFormatHTMLDisplay *
			(*get_html_display)	(EMailReader *reader);
	MessageList *	(*get_message_list)	(EMailReader *reader);
	EShellModule *	(*get_shell_module)	(EMailReader *reader);
	GtkWindow *	(*get_window)		(EMailReader *reader);

	void		(*set_folder)		(EMailReader *reader,
						 CamelFolder *folder,
						 const gchar *folder_uri);
	void		(*set_message)		(EMailReader *reader,
						 const gchar *uid,
						 gboolean mark_read);
};

GType		e_mail_reader_get_type		(void);
void		e_mail_reader_init		(EMailReader *reader);
void		e_mail_reader_changed		(EMailReader *reader);
guint32		e_mail_reader_check_state	(EMailReader *reader);
void		e_mail_reader_update_actions	(EMailReader *reader);
GtkAction *	e_mail_reader_get_action	(EMailReader *reader,
						 const gchar *action_name);
GtkActionGroup *
		e_mail_reader_get_action_group	(EMailReader *reader);
gboolean	e_mail_reader_get_hide_deleted	(EMailReader *reader);
EMFormatHTMLDisplay *
		e_mail_reader_get_html_display	(EMailReader *reader);
MessageList *	e_mail_reader_get_message_list	(EMailReader *reader);
EShellModule *	e_mail_reader_get_shell_module	(EMailReader *reader);
GtkWindow *	e_mail_reader_get_window	(EMailReader *reader);
void		e_mail_reader_set_folder	(EMailReader *reader,
						 CamelFolder *folder,
						 const gchar *folder_uri);
void		e_mail_reader_set_folder_uri	(EMailReader *reader,
						 const gchar *folder_uri);
void		e_mail_reader_set_message	(EMailReader *reader,
						 const gchar *uid,
						 gboolean mark_read);
void		e_mail_reader_create_charset_menu
						(EMailReader *reader,
						 GtkUIManager *ui_manager,
						 guint merge_id);

G_END_DECLS

#endif /* E_MAIL_READER_H */
