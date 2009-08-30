/*
 * Evolution calendar - Event editor dialog
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
 *		Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EVENT_EDITOR_H__
#define __EVENT_EDITOR_H__

#include <gtk/gtk.h>
#include "comp-editor.h"

/* Standard GObject macros */
#define TYPE_EVENT_EDITOR \
	(event_editor_get_type ())
#define EVENT_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_EVENT_EDITOR, EventEditor))
#define EVENT_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_EVENT_EDITOR, EventEditorClass))
#define IS_EVENT_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_EVENT_EDITOR))
#define IS_EVENT_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), TYPE_EVENT_EDITOR))
#define EVENT_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_EVENT_EDITOR, EventEditorClass))

G_BEGIN_DECLS

typedef struct _EventEditor EventEditor;
typedef struct _EventEditorClass EventEditorClass;
typedef struct _EventEditorPrivate EventEditorPrivate;

struct _EventEditor {
	CompEditor parent;
	EventEditorPrivate *priv;
};

struct _EventEditorClass {
	CompEditorClass parent_class;
};

GType		event_editor_get_type		(void);
CompEditor *	event_editor_new		(ECal *client,
						 EShell *shell,
						 CompEditorFlags flags);
void		event_editor_show_meeting	(EventEditor *ee);

G_END_DECLS

#endif /* __EVENT_EDITOR_H__ */
