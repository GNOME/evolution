/*
 * e-mail-config-page.h
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

#ifndef E_MAIL_CONFIG_PAGE_H
#define E_MAIL_CONFIG_PAGE_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_PAGE \
	(e_mail_config_page_get_type ())
#define E_MAIL_CONFIG_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_PAGE, EMailConfigPage))
#define E_IS_MAIL_CONFIG_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_PAGE))
#define E_MAIL_CONFIG_PAGE_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_MAIL_CONFIG_PAGE, EMailConfigPageInterface))

G_BEGIN_DECLS

typedef struct _EMailConfigPage EMailConfigPage;
typedef struct _EMailConfigPageInterface EMailConfigPageInterface;

struct _EMailConfigPageInterface {
	GTypeInterface parent_interface;

	gint sort_order;
	const gchar *title;
	GtkAssistantPageType page_type;

	/* Signals */
	void		(*changed)		(EMailConfigPage *page);
	void		(*setup_defaults)	(EMailConfigPage *page);
	gboolean	(*check_complete)	(EMailConfigPage *page);
	void		(*commit_changes)	(EMailConfigPage *page,
						 GQueue *source_queue);

	/* Intended for pages with server-side settings.
	 * Called after client-side settings are written. */
	gboolean	(*submit_sync)		(EMailConfigPage *page,
						 GCancellable *cancellable,
						 GError **error);
	void		(*submit)		(EMailConfigPage *page,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
	gboolean	(*submit_finish)	(EMailConfigPage *page,
						 GAsyncResult *result,
						 GError **error);
};

GType		e_mail_config_page_get_type	(void) G_GNUC_CONST;
void		e_mail_config_page_set_content	(EMailConfigPage *page,
						 GtkWidget *content);
gint		e_mail_config_page_compare	(GtkWidget *page_a,
						 GtkWidget *page_b);
void		e_mail_config_page_changed	(EMailConfigPage *page);
void		e_mail_config_page_setup_defaults
						(EMailConfigPage *page);
gboolean	e_mail_config_page_check_complete
						(EMailConfigPage *page);
void		e_mail_config_page_commit_changes
						(EMailConfigPage *page,
						 GQueue *source_queue);
gboolean	e_mail_config_page_submit_sync	(EMailConfigPage *page,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_config_page_submit	(EMailConfigPage *page,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_config_page_submit_finish
						(EMailConfigPage *page,
						 GAsyncResult *result,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_CONFIG_PAGE_H */

