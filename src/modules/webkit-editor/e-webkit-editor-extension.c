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

#include "evolution-config.h"

#include "e-webkit-editor-extension.h"
#include "e-webkit-editor.h"

#include <e-util/e-util.h>

struct _EWebKitEditorExtensionPrivate {
	EWebKitEditor *wk_editor;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EWebKitEditorExtension, e_webkit_editor_extension, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (EWebKitEditorExtension))

static void
e_webkit_editor_extension_init (EWebKitEditorExtension *editor_extension)
{
	editor_extension->priv = e_webkit_editor_extension_get_instance_private (editor_extension);

	editor_extension->priv->wk_editor = g_object_ref_sink (e_webkit_editor_new ());
}

static void
webkit_editor_extension_constructed (GObject *object)
{
	EWebKitEditorExtension *self = E_WEBKIT_EDITOR_EXTENSION (object);
	EExtensible *extensible;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_webkit_editor_extension_parent_class)->constructed (object);

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	e_html_editor_register_content_editor (E_HTML_EDITOR (extensible),
		DEFAULT_CONTENT_EDITOR_NAME, E_CONTENT_EDITOR (self->priv->wk_editor));
}

static void
webkit_editor_extension_dispose (GObject *object)
{
	EWebKitEditorExtension *self = E_WEBKIT_EDITOR_EXTENSION (object);

	g_clear_object (&self->priv->wk_editor);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_webkit_editor_extension_parent_class)->dispose (object);
}

static void
e_webkit_editor_extension_class_init (EWebKitEditorExtensionClass *class)
{
	EExtensionClass *extension_class;
	GObjectClass *object_class;

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
