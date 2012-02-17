/*
 * e-mail-config-sendmail-backend.c
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

#include "e-mail-config-sendmail-backend.h"

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigSendmailBackend,
	e_mail_config_sendmail_backend,
	E_TYPE_MAIL_CONFIG_SERVICE_BACKEND)

static void
e_mail_config_sendmail_backend_class_init (EMailConfigSendmailBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "sendmail";

	/* No extra widgets for this backend. */
}

static void
e_mail_config_sendmail_backend_class_finalize (EMailConfigSendmailBackendClass *class)
{
}

static void
e_mail_config_sendmail_backend_init (EMailConfigSendmailBackend *backend)
{
}

void
e_mail_config_sendmail_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_sendmail_backend_register_type (type_module);
}

