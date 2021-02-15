/*
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
 * Authors:
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_EVENT_H
#define EM_EVENT_H

#include <composer/e-msg-composer.h>

/* Standard GObject macros */
#define EM_TYPE_EVENT \
	(em_event_get_type ())
#define EM_EVENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_EVENT, EMEvent))
#define EM_EVENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_EVENT, EMEventClass))
#define EM_IS_EVENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_EVENT))
#define EM_IS_EVENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_EVENT))
#define EM_EVENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_EVENT, EMEventClass))

G_BEGIN_DECLS

typedef struct _EMEvent EMEvent;
typedef struct _EMEventClass EMEventClass;
typedef struct _EMEventPrivate EMEventPrivate;

/* Current target description */
enum _em_event_target_t {
	EM_EVENT_TARGET_FOLDER,
	EM_EVENT_TARGET_MESSAGE,
	EM_EVENT_TARGET_COMPOSER,
	EM_EVENT_TARGET_SEND_RECEIVE,
	EM_EVENT_TARGET_CUSTOM_ICON,
	EM_EVENT_TARGET_FOLDER_UNREAD
};

/* Flags that describe TARGET_FOLDER */
enum {
	EM_EVENT_FOLDER_NEWMAIL = 1<< 0
};

/* Flags that describe TARGET_MESSAGE */
enum {
	EM_EVENT_MESSAGE_REPLY_ALL = 1<< 0,
	EM_EVENT_MESSAGE_REPLY = 1<< 1
};

/* Flags that describe TARGET_COMPOSER */
enum {
	EM_EVENT_COMPOSER_SEND_OPTION = 1<< 0
};

/* Flags that describe TARGET_SEND_RECEIVE*/
enum {
	EM_EVENT_SEND_RECEIVE = 1<< 0
};

/* Flags that describe TARGET_CUSTOM_ICON*/
enum {
	EM_EVENT_CUSTOM_ICON = 1<< 0
};

typedef struct _EMEventTargetFolder EMEventTargetFolder;

struct _EMEventTargetFolder {
	EEventTarget target;
	CamelStore *store;
	gchar *folder_name;
	guint new;
	gboolean is_inbox;
	gchar *display_name;
	gchar *full_display_name;

	/* valid (non-NULL) when only one new message reported */
	gchar *msg_uid;
	gchar *msg_sender;
	gchar *msg_subject;
};

typedef struct _EMEventTargetFolderUnread EMEventTargetFolderUnread;

struct _EMEventTargetFolderUnread {
	EEventTarget target;
	CamelStore *store;
	gchar *folder_uri;
	guint unread;
	gboolean is_inbox;
};

typedef struct _EMEventTargetMessage EMEventTargetMessage;

struct _EMEventTargetMessage {
	EEventTarget target;
	CamelFolder *folder;
	gchar *uid;
	CamelMimeMessage *message;
	EMsgComposer *composer;
};

typedef struct _EMEventTargetComposer EMEventTargetComposer;

struct _EMEventTargetComposer {
	EEventTarget target;
	EMsgComposer *composer;
};

typedef struct _EMEventTargetSendReceive EMEventTargetSendReceive;

struct _EMEventTargetSendReceive {
	EEventTarget target;
	GtkWidget *grid;
	gpointer data;
	gint row;
};

typedef struct _EMEventTargetCustomIcon EMEventTargetCustomIcon;

struct _EMEventTargetCustomIcon {
	EEventTarget target;
	GtkTreeStore *store;
	GtkTreeIter *iter;
	const gchar *folder_name;
};

typedef struct _EEventItem EMEventItem;

/* The object */
struct _EMEvent {
	EEvent popup;
	EMEventPrivate *priv;
};

struct _EMEventClass {
	EEventClass popup_class;
};

GType		em_event_get_type		(void);
EMEvent *	em_event_peek			(void);
EMEventTargetFolder *
		em_event_target_new_folder	(EMEvent *emp,
						 CamelStore *store,
						 const gchar *folder_name,
						 guint32 count_new_msgs,
						 const gchar *msg_uid,
						 const gchar *msg_sender,
						 const gchar *msg_subject);
EMEventTargetFolderUnread *
		em_event_target_new_folder_unread
						(EMEvent *emp,
						 CamelStore *store,
						 const gchar *folder_uri,
						 guint32 count_unread_msgs);
EMEventTargetComposer *
		em_event_target_new_composer	(EMEvent *emp,
						 EMsgComposer *composer,
						 guint32 flags);
EMEventTargetMessage *
		em_event_target_new_message	(EMEvent *emp,
						 CamelFolder *folder,
						 CamelMimeMessage *message,
						 const gchar *uid,
						 guint32 flags,
						 EMsgComposer *composer);
EMEventTargetSendReceive *
		em_event_target_new_send_receive
						(EMEvent *eme,
						 GtkWidget *grid,
						 gpointer data,
						 gint row,
						 guint32 flags);
EMEventTargetCustomIcon *
		em_event_target_new_custom_icon	(EMEvent *eme,
						 GtkTreeStore *store,
						 GtkTreeIter *iter,
						 const gchar *uri,
						 guint32 flags);

G_END_DECLS

#endif /* EM_EVENT_H */
