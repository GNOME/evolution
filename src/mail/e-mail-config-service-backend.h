/*
 * e-mail-config-service-backend.h
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

#ifndef E_MAIL_CONFIG_SERVICE_BACKEND_H
#define E_MAIL_CONFIG_SERVICE_BACKEND_H

#include <gtk/gtk.h>
#include <camel/camel.h>
#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_SERVICE_BACKEND \
	(e_mail_config_service_backend_get_type ())
#define E_MAIL_CONFIG_SERVICE_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_SERVICE_BACKEND, EMailConfigServiceBackend))
#define E_MAIL_CONFIG_SERVICE_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_SERVICE_BACKEND, EMailConfigServiceBackendClass))
#define E_IS_MAIL_CONFIG_SERVICE_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_SERVICE_BACKEND))
#define E_IS_MAIL_CONFIG_SERVICE_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_SERVICE_BACKEND))
#define E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_SERVICE_BACKEND, EMailConfigServiceBackendClass))

G_BEGIN_DECLS

struct _EMailConfigServicePage;

typedef struct _EMailConfigServiceBackend EMailConfigServiceBackend;
typedef struct _EMailConfigServiceBackendClass EMailConfigServiceBackendClass;
typedef struct _EMailConfigServiceBackendPrivate EMailConfigServiceBackendPrivate;

struct _EMailConfigServiceBackend {
	EExtension parent;
	EMailConfigServiceBackendPrivate *priv;
};

struct _EMailConfigServiceBackendClass {
	EExtensionClass parent_class;

	const gchar *backend_name;

	gboolean	(*get_selectable)
					(EMailConfigServiceBackend *backend);
	ESource *	(*new_collection)
					(EMailConfigServiceBackend *backend);
	void		(*insert_widgets)
					(EMailConfigServiceBackend *backend,
					 GtkBox *parent);
	void		(*setup_defaults)
					(EMailConfigServiceBackend *backend);
	gboolean	(*auto_configure)
					(EMailConfigServiceBackend *backend,
					 EConfigLookup *config_lookup,
					 gint *out_priority,
					 gboolean *out_is_complete);
	gboolean	(*check_complete)
					(EMailConfigServiceBackend *backend);
	void		(*commit_changes)
					(EMailConfigServiceBackend *backend);
};

GType		e_mail_config_service_backend_get_type
					(void) G_GNUC_CONST;
struct _EMailConfigServicePage *
		e_mail_config_service_backend_get_page
					(EMailConfigServiceBackend *backend);
ESource *	e_mail_config_service_backend_get_source
					(EMailConfigServiceBackend *backend);
void		e_mail_config_service_backend_set_source
					(EMailConfigServiceBackend *backend,
					 ESource *source);
ESource *	e_mail_config_service_backend_get_collection
					(EMailConfigServiceBackend *backend);
void		e_mail_config_service_backend_set_collection
					(EMailConfigServiceBackend *backend,
					 ESource *collection);
CamelProvider *	e_mail_config_service_backend_get_provider
					(EMailConfigServiceBackend *backend);
CamelSettings *	e_mail_config_service_backend_get_settings
					(EMailConfigServiceBackend *backend);
gboolean	e_mail_config_service_backend_get_selectable
					(EMailConfigServiceBackend *backend);
void		e_mail_config_service_backend_insert_widgets
					(EMailConfigServiceBackend *backend,
					 GtkBox *parent);
void		e_mail_config_service_backend_setup_defaults
					(EMailConfigServiceBackend *backend);
gboolean	e_mail_config_service_backend_auto_configure
					(EMailConfigServiceBackend *backend,
					 EConfigLookup *config_lookup,
					 gint *out_priority,
					 gboolean *out_is_complete);
gboolean	e_mail_config_service_backend_check_complete
					(EMailConfigServiceBackend *backend);
void		e_mail_config_service_backend_commit_changes
					(EMailConfigServiceBackend *backend);
gboolean	e_mail_config_service_backend_auto_configure_for_kind
					(EMailConfigServiceBackend *backend,
					 EConfigLookup *config_lookup,
					 EConfigLookupResultKind kind,
					 const gchar *protocol,
					 ESource *source,
					 gint *out_priority,
					 gboolean *out_is_complete);

G_END_DECLS

#endif /* E_MAIL_CONFIG_SERVICE_BACKEND_H */

