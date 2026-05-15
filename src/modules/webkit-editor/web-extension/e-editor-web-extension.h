/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
