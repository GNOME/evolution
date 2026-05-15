/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-editor-web-extension.h"

/* Forward declaration */
G_MODULE_EXPORT void webkit_web_extension_initialize_with_user_data (WebKitWebExtension *wk_extension,
								     GVariant *user_data);

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *wk_extension,
						GVariant *user_data)
{
	EEditorWebExtension *extension;

	extension = e_editor_web_extension_get_default ();
	e_editor_web_extension_initialize (extension, wk_extension);
}
