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

#define MESSAGE_TAG_EDITOR(obj)	        GTK_CHECK_CAST (obj, message_tag_editor_get_type (), MessageTagEditor)
#define MESSAGE_TAG_EDITOR_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, message_tag_editor_get_type (), MessageTagEditorClass)
#define IS_MESSAGE_TAG_EDITOR(obj)      GTK_CHECK_TYPE (obj, message_tag_editor_get_type ())

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
