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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkstock.h>

#include "message-tag-editor.h"

static void message_tag_editor_class_init (MessageTagEditorClass *class);
static void message_tag_editor_init (MessageTagEditor *editor);
static void message_tag_editor_finalise (GObject *obj);

static CamelTag *get_tag_list (MessageTagEditor *editor);
static void set_tag_list (MessageTagEditor *editor, CamelTag *value);

static GtkDialogClass *parent_class = NULL;

GType
message_tag_editor_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (MessageTagEditorClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) message_tag_editor_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (MessageTagEditor),
			0,
			(GInstanceInitFunc) message_tag_editor_init,
		};
		
		type = g_type_register_static (gtk_dialog_get_type (), "MessageTagEditor", &info, 0);
	}
	
	return type;
}

static void
message_tag_editor_class_init (MessageTagEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (gtk_dialog_get_type ());
	
	object_class->finalize = message_tag_editor_finalise;
	
	klass->get_tag_list = get_tag_list;
	klass->set_tag_list = set_tag_list;
}

static void
message_tag_editor_init (MessageTagEditor *editor)
{
	gtk_window_set_default_size((GtkWindow *)editor, 400, 500);
	gtk_dialog_add_buttons (GTK_DIALOG (editor),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	
	gtk_dialog_set_default_response (GTK_DIALOG (editor), GTK_RESPONSE_OK);
}


static void
message_tag_editor_finalise (GObject *obj)
{
	/*MessageTagEditor *editor = (MessageTagEditor *) obj;*/
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static CamelTag *
get_tag_list (MessageTagEditor *editor)
{
	return NULL;
}

CamelTag *
message_tag_editor_get_tag_list (MessageTagEditor *editor)
{
	g_return_val_if_fail (IS_MESSAGE_TAG_EDITOR (editor), NULL);
	
	return MESSAGE_TAG_EDITOR_GET_CLASS (editor)->get_tag_list (editor);
}


static void
set_tag_list (MessageTagEditor *editor, CamelTag *tags)
{
	/* no-op */
	;
}

void
message_tag_editor_set_tag_list (MessageTagEditor *editor, CamelTag *tags)
{
	g_return_if_fail (IS_MESSAGE_TAG_EDITOR (editor));
	g_return_if_fail (tags != NULL);
	
	MESSAGE_TAG_EDITOR_GET_CLASS (editor)->set_tag_list (editor, tags);
}
