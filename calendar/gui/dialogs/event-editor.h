/* Evolution calendar - Event editor dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EVENT_EDITOR_H__
#define __EVENT_EDITOR_H__

#include <gtk/gtkobject.h>
#include "comp-editor.h"



#define TYPE_EVENT_EDITOR            (event_editor_get_type ())
#define EVENT_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_EVENT_EDITOR, EventEditor))
#define EVENT_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_EVENT_EDITOR,	EventEditorClass))
#define IS_EVENT_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_EVENT_EDITOR))
#define IS_EVENT_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_EVENT_EDITOR))

typedef struct _EventEditor EventEditor;
typedef struct _EventEditorClass EventEditorClass;
typedef struct _EventEditorPrivate EventEditorPrivate;

struct _EventEditor {
	CompEditor parent;

	/* Private data */
	EventEditorPrivate *priv;
};

struct _EventEditorClass {
	CompEditorClass parent_class;
};

GtkType      event_editor_get_type     (void);
EventEditor *event_editor_construct    (EventEditor *ee,
					ECal   *client);
EventEditor *event_editor_new          (ECal   *client, gboolean is_meeting);
void         event_editor_show_meeting (EventEditor *ee);



#endif /* __EVENT_EDITOR_H__ */
