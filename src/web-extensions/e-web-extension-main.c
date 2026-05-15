/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-web-extension.h"

/* Forward declaration */
G_MODULE_EXPORT void webkit_web_extension_initialize_with_user_data (WebKitWebExtension *wk_extension,
								     GVariant *user_data);

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *wk_extension,
						GVariant *user_data)
{
	EWebExtension *extension;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	extension = e_web_extension_get ();

	e_web_extension_initialize (extension, wk_extension);
}
