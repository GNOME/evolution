/*
 * e-mail-display-popup-extension.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "e-mail-display-popup-extension.h"
#include "e-mail-display.h"

G_DEFINE_INTERFACE (
	EMailDisplayPopupExtension,
	e_mail_display_popup_extension,
	G_TYPE_OBJECT)

static void
e_mail_display_popup_extension_default_init (EMailDisplayPopupExtensionInterface *iface)
{

}

/**
 * e_mail_display_popup_extension_update_actions:
 *
 * @extension: An object derived from #EMailDisplayPopupExtension
 * @popup_iframe_src: iframe source URI on top of which the popup menu had been invoked
 * @popup_iframe_id: iframe ID on top of which the popup menu had been invoked
 *
 * When #EMailDisplay is about to display a popup menu, it calls this function
 * on every extension so that they can add their items to the menu.
 */
void
e_mail_display_popup_extension_update_actions (EMailDisplayPopupExtension *extension,
					       const gchar *popup_iframe_src,
					       const gchar *popup_iframe_id)
{
	EMailDisplayPopupExtensionInterface *iface;

	g_return_if_fail (E_IS_MAIL_DISPLAY_POPUP_EXTENSION (extension));

	iface = E_MAIL_DISPLAY_POPUP_EXTENSION_GET_INTERFACE (extension);
	g_return_if_fail (iface->update_actions != NULL);

	iface->update_actions (extension, popup_iframe_src, popup_iframe_id);
}
