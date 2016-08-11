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

#ifndef E_WEBKIT_EDITOR_EXTENSION_H
#define E_WEBKIT_EDITOR_EXTENSION_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_WEBKIT_EDITOR_EXTENSION \
	(e_webkit_editor_extension_get_type ())
#define E_WEBKIT_EDITOR_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEBKIT_EDITOR_EXTENSION, EWebKitEditorExtension))
#define E_WEBKIT_EDITOR_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEBKIT_EDITOR_EXTENSION, EWebKitEditorExtensionClass))
#define E_IS_WEBKIT_EDITOR_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEBKIT_EDITOR_EXTENSION))
#define E_IS_WEBKIT_EDITOR_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEBKIT_EDITOR_EXTENSION))
#define E_WEBKIT_EDITOR_EXTENSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEBKIT_EDITOR_EXTENSION, EWebKitEditorExtensionClass))

G_BEGIN_DECLS

typedef struct _EWebKitEditorExtension EWebKitEditorExtension;
typedef struct _EWebKitEditorExtensionClass EWebKitEditorExtensionClass;
typedef struct _EWebKitEditorExtensionPrivate EWebKitEditorExtensionPrivate;

struct _EWebKitEditorExtension {
	EExtension parent;

	EWebKitEditorExtensionPrivate *priv;
};

struct _EWebKitEditorExtensionClass {
	EExtensionClass parent_class;
};

GType		e_webkit_editor_extension_get_type	(void) G_GNUC_CONST;
void		e_webkit_editor_extension_type_register	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_WEBKIT_EDITOR_EXTENSION_H */
