/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Michel Zucchi <notzed@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __EM_EVENT_H__
#define __EM_EVENT_H__

#include <glib-object.h>

#include "e-util/e-event.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

struct _CamelFolder;
struct _CamelMimeMessage;

typedef struct _EMEvent EMEvent;
typedef struct _EMEventClass EMEventClass;

/* Current target description */
enum _em_event_target_t {
	EM_EVENT_TARGET_FOLDER,
	EM_EVENT_TARGET_MESSAGE,
};

/* Flags that describe TARGET_FOLDER */
enum {
	EM_EVENT_FOLDER_NEWMAIL = 1<< 0,
};

/* Flags that describe TARGET_MESSAGE */
enum {
	EM_EVENT_MESSAGE_REPLY_ALL = 1<< 0,
};

typedef struct _EMEventTargetFolder EMEventTargetFolder;

struct _EMEventTargetFolder {
	EEventTarget target;
	char *uri;
};

typedef struct _EMEventTargetMessage EMEventTargetMessage;

struct _EMEventTargetMessage {
	EEventTarget              target;
	struct _CamelFolder      *folder;
	char                     *uid;
	struct _CamelMimeMessage *message;
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

EMEventTargetFolder *em_event_target_new_folder(EMEvent *emp, const char *uri, guint32 flags);
EMEventTargetMessage *em_event_target_new_message(EMEvent *emp, struct _CamelFolder *folder, struct _CamelMimeMessage *message, const char *uid, guint32 flags);

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_EVENT_H__ */
