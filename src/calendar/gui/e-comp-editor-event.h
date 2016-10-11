/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_COMP_EDITOR_EVENT_H
#define E_COMP_EDITOR_EVENT_H

#include <calendar/gui/e-comp-editor.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR_EVENT \
	(e_comp_editor_event_get_type ())
#define E_COMP_EDITOR_EVENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_EVENT, ECompEditorEvent))
#define E_COMP_EDITOR_EVENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_EVENT, ECompEditorEventClass))
#define E_IS_COMP_EDITOR_EVENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_EVENT))
#define E_IS_COMP_EDITOR_EVENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_EVENT))
#define E_COMP_EDITOR_EVENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_EVENT, ECompEditorEventClass))

G_BEGIN_DECLS

typedef struct _ECompEditorEvent ECompEditorEvent;
typedef struct _ECompEditorEventClass ECompEditorEventClass;
typedef struct _ECompEditorEventPrivate ECompEditorEventPrivate;

struct _ECompEditorEvent {
	ECompEditor parent;

	ECompEditorEventPrivate *priv;
};

struct _ECompEditorEventClass {
	ECompEditorClass parent_class;
};

GType		e_comp_editor_event_get_type	(void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_COMP_EDITOR_EVENT_H */
