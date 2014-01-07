/*
 * e-mail-config-service-notebook.h
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

#ifndef E_MAIL_CONFIG_SERVICE_NOTEBOOK_H
#define E_MAIL_CONFIG_SERVICE_NOTEBOOK_H

#include <gtk/gtk.h>
#include <mail/e-mail-config-service-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_SERVICE_NOTEBOOK \
	(e_mail_config_service_notebook_get_type ())
#define E_MAIL_CONFIG_SERVICE_NOTEBOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_SERVICE_NOTEBOOK, EMailConfigServiceNotebook))
#define E_MAIL_CONFIG_SERVICE_NOTEBOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_SERVICE_NOTEBOOK, EMailConfigServiceNotebookClass))
#define E_IS_MAIL_CONFIG_SERVICE_NOTEBOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_SERVICE_NOTEBOOK))
#define E_IS_MAIL_CONFIG_SERVICE_NOTEBOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_SERVICE_NOTEBOOK))
#define E_MAIL_CONFIG_SERVICE_NOTEBOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_SERVICE_NOTEBOOK, EMailConfigServiceNotebookClass))

G_BEGIN_DECLS

typedef struct _EMailConfigServiceNotebook EMailConfigServiceNotebook;
typedef struct _EMailConfigServiceNotebookClass EMailConfigServiceNotebookClass;
typedef struct _EMailConfigServiceNotebookPrivate EMailConfigServiceNotebookPrivate;

struct _EMailConfigServiceNotebook {
	GtkNotebook parent;
	EMailConfigServiceNotebookPrivate *priv;
};

struct _EMailConfigServiceNotebookClass {
	GtkNotebookClass parent_class;
};

GType		e_mail_config_service_notebook_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_config_service_notebook_new
					(void);
gint		e_mail_config_service_notebook_add_page
					(EMailConfigServiceNotebook *notebook,
					 EMailConfigServiceBackend *backend,
					 GtkWidget *child);
EMailConfigServiceBackend *
		e_mail_config_service_notebook_get_active_backend
					(EMailConfigServiceNotebook *notebook);
void		e_mail_config_service_notebook_set_active_backend
					(EMailConfigServiceNotebook *notebook,
					 EMailConfigServiceBackend *backend);

G_END_DECLS

#endif /* E_MAIL_CONFIG_SERVICE_NOTEBOOK_H */

