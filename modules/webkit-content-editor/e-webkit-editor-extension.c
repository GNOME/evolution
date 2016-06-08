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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-webkit-editor-extension.h"
#include "e-webkit-content-editor.h"

#include <e-util/e-util.h>

#define E_WEBKIT_EDITOR_EXTENSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEBKIT_EDITOR_EXTENSION, EWebKitEditorExtensionPrivate))

struct _EWebKitEditorExtensionPrivate {
	EWebKitContentEditor *wk_editor;
};

G_DEFINE_DYNAMIC_TYPE (
	EWebKitEditorExtension,
	e_webkit_editor_extension,
	E_TYPE_EXTENSION)

static void
e_webkit_editor_extension_init (EWebKitEditorExtension *editor_extension)
{
	editor_extension->priv = E_WEBKIT_EDITOR_EXTENSION_GET_PRIVATE (editor_extension);

	editor_extension->priv->wk_editor = g_object_ref_sink (e_webkit_content_editor_new ());
}

static void
webkit_editor_extension_constructed (GObject *object)
{
	EWebKitEditorExtensionPrivate *priv;
	EExtensible *extensible;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_webkit_editor_extension_parent_class)->constructed (object);

	priv = E_WEBKIT_EDITOR_EXTENSION_GET_PRIVATE (object);
	extensible = e_extension_get_extensible (E_EXTENSION (object));

	e_html_editor_register_content_editor (E_HTML_EDITOR (extensible),
		DEFAULT_CONTENT_EDITOR_NAME, E_CONTENT_EDITOR (priv->wk_editor));
}

static void
webkit_editor_extension_dispose (GObject *object)
{
	EWebKitEditorExtensionPrivate *priv;

	priv = E_WEBKIT_EDITOR_EXTENSION_GET_PRIVATE (object);

	g_clear_object (&priv->wk_editor);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_webkit_editor_extension_parent_class)->dispose (object);
}

static void
e_webkit_editor_extension_class_init (EWebKitEditorExtensionClass *class)
{
	EExtensionClass *extension_class;
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EWebKitEditorExtensionPrivate));

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_HTML_EDITOR;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = webkit_editor_extension_dispose;
	object_class->constructed = webkit_editor_extension_constructed;
}

static void
e_webkit_editor_extension_class_finalize (EWebKitEditorExtensionClass *class)
{
}

void
e_webkit_editor_extension_type_register (GTypeModule *type_module)
{
	e_webkit_editor_extension_register_type (type_module);
}
