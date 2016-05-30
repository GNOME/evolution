/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_H
#define E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_H

#include <webkit2/webkit2.h>

/* Standard GObject macros */
#define E_TYPE_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER \
	(e_webkit_content_editor_find_controller_get_type ())
#define E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER, EWebKitContentEditorFindController))
#define E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER, EWebKitContentEditorFindControllerClass))
#define E_IS_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER))
#define E_IS_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER))
#define E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER, EWebKitContentEditorFindControllerClass))

G_BEGIN_DECLS

typedef struct _EWebKitContentEditorFindController EWebKitContentEditorFindController;
typedef struct _EWebKitContentEditorFindControllerClass EWebKitContentEditorFindControllerClass;
typedef struct _EWebKitContentEditorFindControllerPrivate EWebKitContentEditorFindControllerPrivate;

struct _EWebKitContentEditorFindController {
	GObject parent;

	EWebKitContentEditorFindControllerPrivate *priv;
};

struct _EWebKitContentEditorFindControllerClass {
	GObjectClass parent_class;
};

GType		e_webkit_content_editor_find_controller_get_type
							(void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_H */
