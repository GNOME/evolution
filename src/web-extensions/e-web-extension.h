/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_WEB_EXTENSION_H
#define E_WEB_EXTENSION_H

#include <glib-object.h>
#include <webkit2/webkit-web-extension.h>

/* Standard GObject macros */
#define E_TYPE_WEB_EXTENSION \
	(e_web_extension_get_type ())
#define E_WEB_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEB_EXTENSION, EWebExtension))
#define E_WEB_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEB_EXTENSION, EWebExtensionClass))
#define E_IS_WEB_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEB_EXTENSION))
#define E_IS_WEB_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEB_EXTENSION))
#define E_WEB_EXTENSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEB_EXTENSION, EWebExtensionClass))

G_BEGIN_DECLS

typedef struct _EWebExtension EWebExtension;
typedef struct _EWebExtensionClass EWebExtensionClass;
typedef struct _EWebExtensionPrivate EWebExtensionPrivate;

struct _EWebExtension {
	GObject parent;
	EWebExtensionPrivate *priv;
};

struct _EWebExtensionClass
{
	GObjectClass parent_class;
};

GType		e_web_extension_get_type	(void) G_GNUC_CONST;

EWebExtension *	e_web_extension_get		(void);

void		e_web_extension_initialize	(EWebExtension *extension,
						 WebKitWebExtension *wk_extension);

WebKitWebExtension *
		e_web_extension_get_webkit_extension
						(EWebExtension *extension);
G_END_DECLS

#endif /* E_WEB_EXTENSION_H */
