/*
 * e-mail-config-restore-ready-page.c
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

/* NOTE: This page is never actually shown to the user.  It works as a
 *       placeholder, visible only when the user chooses a backup file
 *       to restore.  As soon as we arrive on this page we execl() the
 *       "evolution-backup" tool, and the startup wizard disappears. */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-mail-config-restore-ready-page.h"

/* Forward Declarations */
static void	e_mail_config_restore_ready_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailConfigRestoreReadyPage,
	e_mail_config_restore_ready_page,
	GTK_TYPE_SCROLLED_WINDOW,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_CONFIG_PAGE,
		e_mail_config_restore_ready_page_interface_init))

static void
e_mail_config_restore_ready_page_class_init (EMailConfigRestoreReadyPageClass *class)
{
}

static void
e_mail_config_restore_ready_page_class_finalize (EMailConfigRestoreReadyPageClass *class)
{
}

static void
e_mail_config_restore_ready_page_interface_init (EMailConfigPageInterface *iface)
{
	/* Keep the title identical to EMailConfigRestorePage
	 * so it's only shown once in the assistant sidebar. */
	iface->title = _("Restore from Backup");
	iface->sort_order = E_MAIL_CONFIG_RESTORE_READY_PAGE_SORT_ORDER;
}

static void
e_mail_config_restore_ready_page_init (EMailConfigRestoreReadyPage *page)
{
}

void
e_mail_config_restore_ready_page_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_restore_ready_page_register_type (type_module);
}

EMailConfigPage *
e_mail_config_restore_ready_page_new (void)
{
	return g_object_new (E_TYPE_MAIL_CONFIG_RESTORE_READY_PAGE, NULL);
}

