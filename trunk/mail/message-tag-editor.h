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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */


#ifndef __MESSAGE_TAG_EDITOR_H__
#define __MESSAGE_TAG_EDITOR_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtkdialog.h>
#include <camel/camel-folder.h>
#include <camel/camel-folder-summary.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define MESSAGE_TAG_EDITOR_TYPE            (message_tag_editor_get_type ())
#define MESSAGE_TAG_EDITOR(obj)	           (G_TYPE_CHECK_INSTANCE_CAST (obj, MESSAGE_TAG_EDITOR_TYPE, MessageTagEditor))
#define MESSAGE_TAG_EDITOR_CLASS(klass)	   (G_TYPE_CHECK_CLASS_CAST (klass, MESSAGE_TAG_EDITOR_TYPE, MessageTagEditorClass))
#define IS_MESSAGE_TAG_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, MESSAGE_TAG_EDITOR_TYPE))
#define IS_MESSAGE_TAG_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MESSAGE_TAG_EDITOR_TYPE))
#define MESSAGE_TAG_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MESSAGE_TAG_EDITOR_TYPE, MessageTagEditorClass))

typedef struct _MessageTagEditor MessageTagEditor;
typedef struct _MessageTagEditorClass MessageTagEditorClass;

struct _MessageTagEditor {
	GtkDialog parent;

};

struct _MessageTagEditorClass {
	GtkDialogClass parent_class;

	/* virtual methods */
	CamelTag * (*get_tag_list) (MessageTagEditor *editor);
	void       (*set_tag_list) (MessageTagEditor *editor, CamelTag *tags);

	/* signals */
};


GtkType message_tag_editor_get_type (void);

/* methods */
CamelTag *message_tag_editor_get_tag_list (MessageTagEditor *editor);
void      message_tag_editor_set_tag_list (MessageTagEditor *editor, CamelTag *tags);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MESSAGE_TAG_EDITOR_H__ */
