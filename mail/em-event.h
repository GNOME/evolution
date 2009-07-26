/*
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
 * Authors:
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_EVENT_H__
#define __EM_EVENT_H__

#include <glib-object.h>

#include "e-util/e-event.h"
#include "composer/e-msg-composer.h"
#include "mail/em-folder-browser.h"

G_BEGIN_DECLS

typedef struct _EMEvent EMEvent;
typedef struct _EMEventClass EMEventClass;

/* Current target description */
enum _em_event_target_t {
	EM_EVENT_TARGET_FOLDER,
	EM_EVENT_TARGET_FOLDER_BROWSER,
	EM_EVENT_TARGET_MESSAGE,
	EM_EVENT_TARGET_COMPOSER,
	EM_EVENT_TARGET_SEND_RECEIVE,
	EM_EVENT_TARGET_CUSTOM_ICON
};

/* Flags for FOLDER BROWSER Events*/
enum {
	EM_EVENT_FOLDER_BROWSER = 1<< 0
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
	gchar *uri;
	guint  new;
	gboolean is_inbox;
	gchar *name;
};

typedef struct _EMEventTargetMessage EMEventTargetMessage;

struct _EMEventTargetMessage {
	EEventTarget              target;
	CamelFolder      *folder;
	gchar                     *uid;
	CamelMimeMessage *message;
	EMsgComposer *composer;
};

typedef struct _EMEventTargetComposer EMEventTargetComposer;

struct _EMEventTargetComposer {
	EEventTarget target;

	EMsgComposer *composer;
};

typedef struct _EMEventTargetFolderBrowser EMEventTargetFolderBrowser;

struct _EMEventTargetFolderBrowser {
	EEventTarget target;

	EMFolderBrowser *emfb;
};

typedef struct _EMEventTargetSendReceive EMEventTargetSendReceive;

struct _EMEventTargetSendReceive {
	EEventTarget target;

	GtkWidget *table;
	gpointer data;
	gint row;
};

typedef struct _EMEventTargetCustomIcon EMEventTargetCustomIcon;

struct _EMEventTargetCustomIcon {
	EEventTarget target;

	GtkTreeStore	*store;
	GtkTreeIter	*iter;
	const gchar	*folder_name;
};

typedef struct _EEventItem EMEventItem;

/* The object */
struct _EMEvent {
	EEvent popup;

	struct _EMEventPrivate *priv;
};

struct _EMEventClass {
	EEventClass popup_class;
};

GType em_event_get_type(void);

EMEvent *em_event_peek(void);

EMEventTargetFolder *em_event_target_new_folder(EMEvent *emp, const gchar *uri, guint32 flags);
EMEventTargetFolderBrowser *em_event_target_new_folder_browser (EMEvent *eme, EMFolderBrowser *emfb);
EMEventTargetComposer *em_event_target_new_composer(EMEvent *emp, const EMsgComposer *composer, guint32 flags);
EMEventTargetMessage *em_event_target_new_message(EMEvent *emp, CamelFolder *folder, CamelMimeMessage *message, const gchar *uid, guint32 flags,
							EMsgComposer *composer);
EMEventTargetSendReceive * em_event_target_new_send_receive(EMEvent *eme, GtkWidget *table, gpointer data, gint row, guint32 flags);
EMEventTargetCustomIcon * em_event_target_new_custom_icon(EMEvent *eme, GtkTreeStore *store, GtkTreeIter *iter, const gchar *uri, guint32 flags);

/* ********************************************************************** */

typedef struct _EMEventHook EMEventHook;
typedef struct _EMEventHookClass EMEventHookClass;

struct _EMEventHook {
	EEventHook hook;
};

struct _EMEventHookClass {
	EEventHookClass hook_class;
};

GType em_event_hook_get_type(void);

G_END_DECLS

#endif /* __EM_EVENT_H__ */
