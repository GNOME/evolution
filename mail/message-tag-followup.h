/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximain, Inc. (www.ximian.com)
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


#ifndef __MESSAGE_TAG_FOLLOWUP_H__
#define __MESSAGE_TAG_FOLLOWUP_H__

#include <gtk/gtk.h>
#include <mail/message-tag-editor.h>
#include <camel/camel-folder.h>
#include <camel/camel-folder-summary.h>
#include <widgets/misc/e-dateedit.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define MESSAGE_TAG_FOLLOWUP(obj)	  GTK_CHECK_CAST (obj, message_tag_followup_get_type (), MessageTagFollowUp)
#define MESSAGE_TAG_FOLLOWUP_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, message_tag_followup_get_type (), MessageTagFollowUpClass)
#define IS_MESSAGE_TAG_FOLLOWUP(obj)      GTK_CHECK_TYPE (obj, message_tag_followup_get_type ())

enum {
	FOLLOWUP_FLAG_CALL,
	FOLLOWUP_FLAG_DO_NOT_FORWARD,
	FOLLOWUP_FLAG_FOLLOWUP,
	FOLLOWUP_FLAG_FYI,
	FOLLOWUP_FLAG_FORWARD,
	FOLLOWUP_FLAG_NO_RESPONSE_NECESSARY,
	FOLLOWUP_FLAG_READ,
	FOLLOWUP_FLAG_REPLY,
	FOLLOWUP_FLAG_REPLY_ALL,
	FOLLOWUP_FLAG_REVIEW,
	FOLLOWUP_FLAG_NONE
};

struct _FollowUpTag {
	int type;
	time_t target_date;
	time_t completed;
};

typedef struct _MessageTagFollowUp MessageTagFollowUp;
typedef struct _MessageTagFollowUpClass MessageTagFollowUpClass;

struct _MessageTagFollowUp {
	MessageTagEditor parent;
	
	struct _FollowUpTag *tag;
	char *value;
	
	GtkCList *message_list;
	
	GtkOptionMenu *type;
	GtkWidget *none;
	
	EDateEdit *target_date;
	GtkToggleButton *completed;
	GtkButton *clear;
};

struct _MessageTagFollowUpClass {
	MessageTagEditorClass parent_class;
	
	/* virtual methods */
	/* signals */
};


GtkType message_tag_followup_get_type (void);

/* utility functions */
struct _FollowUpTag *message_tag_followup_decode (const char *tag_value);
char *message_tag_followup_encode (struct _FollowUpTag *followup);
const char *message_tag_followup_i18n_name (int type);

MessageTagEditor *message_tag_followup_new (void);

void message_tag_followup_append_message (MessageTagFollowUp *editor,
					  const char *from,
					  const char *subject);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MESSAGE_TAG_FOLLOWUP_H__ */
