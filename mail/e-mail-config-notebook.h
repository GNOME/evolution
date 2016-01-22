/*
 * e-mail-config-notebook.h
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

#ifndef E_MAIL_CONFIG_NOTEBOOK_H
#define E_MAIL_CONFIG_NOTEBOOK_H

#include <gtk/gtk.h>
#include <libemail-engine/libemail-engine.h>

#include <mail/e-mail-config-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_NOTEBOOK \
	(e_mail_config_notebook_get_type ())
#define E_MAIL_CONFIG_NOTEBOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_NOTEBOOK, EMailConfigNotebook))
#define E_MAIL_CONFIG_NOTEBOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_NOTEBOOK, EMailConfigNotebookClass))
#define E_IS_MAIL_CONFIG_NOTEBOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_NOTEBOOK))
#define E_IS_MAIL_CONFIG_NOTEBOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_NOTEBOOK))
#define E_MAIL_CONFIG_NOTEBOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_NOTEBOOK, EMailConfigNotebookClass))

G_BEGIN_DECLS

typedef struct _EMailConfigNotebook EMailConfigNotebook;
typedef struct _EMailConfigNotebookClass EMailConfigNotebookClass;
typedef struct _EMailConfigNotebookPrivate EMailConfigNotebookPrivate;

struct _EMailConfigNotebook {
	GtkNotebook parent;
	EMailConfigNotebookPrivate *priv;
};

struct _EMailConfigNotebookClass {
	GtkNotebookClass parent_class;
};

GType		e_mail_config_notebook_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_config_notebook_new
					(EMailSession *session,
					 ESource *original_source,
					 ESource *account_source,
					 ESource *identity_source,
					 ESource *transport_source,
					 ESource *collection_source);
EMailSession *	e_mail_config_notebook_get_session
					(EMailConfigNotebook *notebook);
ESource *	e_mail_config_notebook_get_original_source
					(EMailConfigNotebook *notebook);
ESource *	e_mail_config_notebook_get_account_source
					(EMailConfigNotebook *notebook);
ESource *	e_mail_config_notebook_get_identity_source
					(EMailConfigNotebook *notebook);
ESource *	e_mail_config_notebook_get_transport_source
					(EMailConfigNotebook *notebook);
ESource *	e_mail_config_notebook_get_collection_source
					(EMailConfigNotebook *notebook);
void		e_mail_config_notebook_add_page
					(EMailConfigNotebook *notebook,
					 EMailConfigPage *page);
gboolean	e_mail_config_notebook_check_complete
					(EMailConfigNotebook *notebook);
void		e_mail_config_notebook_commit
					(EMailConfigNotebook *notebook,
					 GCancellable *cancellable,
					 GAsyncReadyCallback callback,
					 gpointer user_data);
gboolean	e_mail_config_notebook_commit_finish
					(EMailConfigNotebook *notebook,
					 GAsyncResult *result,
					 GError **error);

G_END_DECLS

#endif /* E_MAIL_CONFIG_NOTEBOOK_H */

