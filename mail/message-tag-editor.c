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

#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-window-icon.h>
#include "message-tag-editor.h"


static void message_tag_editor_class_init (MessageTagEditorClass *class);
static void message_tag_editor_init (MessageTagEditor *editor);
static void message_tag_editor_finalise (GtkObject *obj);

static const char *tag_get_name  (MessageTagEditor *editor);
static const char *tag_get_value (MessageTagEditor *editor);
static void tag_set_value (MessageTagEditor *editor, const char *value);

static GnomeDialogClass *parent_class;

GtkType
message_tag_editor_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MessageTagEditor",
			sizeof (MessageTagEditor),
			sizeof (MessageTagEditorClass),
			(GtkClassInitFunc) message_tag_editor_class_init,
			(GtkObjectInitFunc) message_tag_editor_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gnome_dialog_get_type (), &type_info);
	}
	
	return type;
}

static void
message_tag_editor_class_init (MessageTagEditorClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;
	parent_class = gtk_type_class (gnome_dialog_get_type ());
	
	object_class->finalize = message_tag_editor_finalise;
	
	klass->get_name = tag_get_name;
	klass->get_value = tag_get_value;
	klass->set_value = tag_set_value;
}

static void
message_tag_editor_init (MessageTagEditor *editor)
{
	gtk_window_set_policy (GTK_WINDOW (editor), FALSE, TRUE, FALSE);
	
	gnome_dialog_append_buttons (GNOME_DIALOG (editor),
				     GNOME_STOCK_BUTTON_OK,
				     GNOME_STOCK_BUTTON_CANCEL,
				     NULL);
	
	gnome_dialog_set_default (GNOME_DIALOG (editor), 0);
}


static void
message_tag_editor_finalise (GtkObject *obj)
{
	/*	MessageTagEditor *editor = (MessageTagEditor *) obj;*/
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}


static const char *
tag_get_name  (MessageTagEditor *editor)
{
	return NULL;
}

const char *
message_tag_editor_get_name (MessageTagEditor *editor)
{
	g_return_val_if_fail (IS_MESSAGE_TAG_EDITOR (editor), NULL);
	
	return ((MessageTagEditorClass *)((GtkObject *) editor)->klass)->get_name (editor);
}


static const char *
tag_get_value (MessageTagEditor *editor)
{
	return NULL;
}

const char *
message_tag_editor_get_value (MessageTagEditor *editor)
{
	g_return_val_if_fail (IS_MESSAGE_TAG_EDITOR (editor), NULL);
	
	return ((MessageTagEditorClass *)((GtkObject *) editor)->klass)->get_value (editor);
}


static void
tag_set_value (MessageTagEditor *editor, const char *value)
{
	/* no-op */
	;
}

void
message_tag_editor_set_value (MessageTagEditor *editor, const char *value)
{
	g_return_if_fail (IS_MESSAGE_TAG_EDITOR (editor));
	g_return_if_fail (value != NULL);
	
	((MessageTagEditorClass *)((GtkObject *) editor)->klass)->set_value (editor, value);
}
