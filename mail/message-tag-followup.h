/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include <mail/message-tag-editor.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define MESSAGE_TAG_FOLLOWUP_TYPE            (message_tag_followup_get_type ())
#define MESSAGE_TAG_FOLLOWUP(obj)	     (G_TYPE_CHECK_INSTANCE_CAST (obj, MESSAGE_TAG_FOLLOWUP_TYPE, MessageTagFollowUp))
#define MESSAGE_TAG_FOLLOWUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, MESSAGE_TAG_FOLLOWUP_TYPE, MessageTagFollowUpClass))
#define IS_MESSAGE_TAG_FOLLOWUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, MESSAGE_TAG_FOLLOWUP_TYPE))
#define IS_MESSAGE_TAG_FOLLOWUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MESSAGE_TAG_FOLLOWUP_TYPE))
#define MESSAGE_TAG_FOLLOWUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MESSAGE_TAG_FOLLOWUP_TYPE, MessageTagFollowUpClass))

typedef struct _MessageTagFollowUp MessageTagFollowUp;
typedef struct _MessageTagFollowUpClass MessageTagFollowUpClass;

struct _MessageTagFollowUp {
	MessageTagEditor parent;
	
	struct _GtkTreeView *message_list;
	
	struct _GtkCombo *combo;
	
	struct _EDateEdit *target_date;
	struct _GtkToggleButton *completed;
	struct _GtkButton *clear;
	
	time_t completed_date;
};

struct _MessageTagFollowUpClass {
	MessageTagEditorClass parent_class;
	
	/* virtual methods */
	/* signals */
};


GtkType message_tag_followup_get_type (void);

MessageTagEditor *message_tag_followup_new (void);

void message_tag_followup_append_message (MessageTagFollowUp *editor,
					  const char *from,
					  const char *subject);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MESSAGE_TAG_FOLLOWUP_H__ */
