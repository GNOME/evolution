/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-mail-config-sending-page.h"

/* Forward Declarations */
static void	e_mail_config_sending_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EMailConfigSendingPage,
	e_mail_config_sending_page,
	E_TYPE_MAIL_CONFIG_SERVICE_PAGE,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_CONFIG_PAGE,
		e_mail_config_sending_page_interface_init))

static void
e_mail_config_sending_page_class_init (EMailConfigSendingPageClass *class)
{
	EMailConfigServicePageClass *service_page_class;

	service_page_class = E_MAIL_CONFIG_SERVICE_PAGE_CLASS (class);
	service_page_class->extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;
	service_page_class->provider_type = CAMEL_PROVIDER_TRANSPORT;
	service_page_class->default_backend_name = "smtp";
}

static void
e_mail_config_sending_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Sending Email");
	iface->sort_order = E_MAIL_CONFIG_SENDING_PAGE_SORT_ORDER;
}

static void
e_mail_config_sending_page_init (EMailConfigSendingPage *page)
{
}

EMailConfigPage *
e_mail_config_sending_page_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_SENDING_PAGE,
		"registry", registry, NULL);
}

