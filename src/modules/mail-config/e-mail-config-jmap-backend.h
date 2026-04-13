/*
 * e-mail-config-jmap-backend.h
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

#ifndef E_MAIL_CONFIG_JMAP_BACKEND_H
#define E_MAIL_CONFIG_JMAP_BACKEND_H

#include <mail/e-mail-config-service-backend.h>

G_BEGIN_DECLS

#define E_TYPE_MAIL_CONFIG_JMAP_BACKEND \
	(e_mail_config_jmap_backend_get_type ())
G_DECLARE_FINAL_TYPE (EMailConfigJmapBackend, e_mail_config_jmap_backend, E, MAIL_CONFIG_JMAP_BACKEND, EMailConfigServiceBackend)

void		e_mail_config_jmap_backend_type_register
					(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MAIL_CONFIG_JMAP_BACKEND_H */
