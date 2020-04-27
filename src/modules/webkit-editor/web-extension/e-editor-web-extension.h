/*
 * e-editor-web-extension.h
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
 */

#ifndef E_EDITOR_WEB_EXTENSION_H
#define E_EDITOR_WEB_EXTENSION_H

#include <glib-object.h>
#include <webkit2/webkit-web-extension.h>

/* Standard GObject macros */
#define E_TYPE_EDITOR_WEB_EXTENSION \
	(e_editor_web_extension_get_type ())
#define E_EDITOR_WEB_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR_WEB_EXTENSION, EEditorWebExtension))
#define E_EDITOR_WEB_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR_WEB_EXTENSION, EEditorWebExtensionClass))
#define E_IS_EDITOR_WEB_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR_WEB_EXTENSION))
#define E_IS_EDITOR_WEB_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR_WEB_EXTENSION))
#define E_EDITOR_WEB_EXTENSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR_WEB_EXTENSION, EEditorWebExtensionClass))

G_BEGIN_DECLS

typedef struct _EEditorWebExtension EEditorWebExtension;
typedef struct _EEditorWebExtensionClass EEditorWebExtensionClass;
typedef struct _EEditorWebExtensionPrivate EEditorWebExtensionPrivate;

struct _EEditorWebExtension {
	GObject parent;
	EEditorWebExtensionPrivate *priv;
};

struct _EEditorWebExtensionClass
{
	GObjectClass parent_class;
};

GType		e_editor_web_extension_get_type	(void) G_GNUC_CONST;

EEditorWebExtension *
		e_editor_web_extension_get_default
						(void);

void		e_editor_web_extension_initialize
						(EEditorWebExtension *extension,
						 WebKitWebExtension *wk_extension);

#endif /* E_EDITOR_WEB_EXTENSION_H */
