/*
 * evolution-imap-features.c
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

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libebackend/e-extension.h>
#include <libedataserver/e-source-mail-account.h>

#include <mail/e-mail-config-notebook.h>

#include "e-mail-config-header-manager.h"
#include "e-mail-config-imap-headers-page.h"

typedef EExtension EvolutionImapFeatures;
typedef EExtensionClass EvolutionImapFeaturesClass;

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType evolution_imap_features_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EvolutionImapFeatures,
	evolution_imap_features,
	E_TYPE_EXTENSION)

static void
evolution_imap_features_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	ESource *source;
	ESourceBackend *backend_ext;
	EMailConfigNotebook *notebook;
	const gchar *backend_name;
	const gchar *extension_name;
	gboolean add_page = FALSE;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (evolution_imap_features_parent_class)->
		constructed (object);

	notebook = E_MAIL_CONFIG_NOTEBOOK (extensible);
	source = e_mail_config_notebook_get_account_source (notebook);

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	backend_ext = e_source_get_extension (source, extension_name);
	backend_name = e_source_backend_get_backend_name (backend_ext);

	if (g_strcmp0 (backend_name, "imap") == 0)
		add_page = TRUE;

	if (add_page) {
		EMailConfigPage *page;
		page = e_mail_config_imap_headers_page_new (source);
		e_mail_config_notebook_add_page (notebook, page);
	}
}

static void
evolution_imap_features_class_init (EvolutionImapFeaturesClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = evolution_imap_features_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_CONFIG_NOTEBOOK;
}

static void
evolution_imap_features_class_finalize (EvolutionImapFeaturesClass *class)
{
}

static void
evolution_imap_features_init (EvolutionImapFeatures *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	evolution_imap_features_register_type (type_module);
	e_mail_config_header_manager_type_register (type_module);
	e_mail_config_imap_headers_page_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

