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
