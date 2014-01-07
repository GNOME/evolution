/*
 * e-mail-account-store.h
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

#ifndef E_MAIL_ACCOUNT_STORE_H
#define E_MAIL_ACCOUNT_STORE_H

#include <gtk/gtk.h>
#include <camel/camel.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_ACCOUNT_STORE \
	(e_mail_account_store_get_type ())
#define E_MAIL_ACCOUNT_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_ACCOUNT_STORE, EMailAccountStore))
#define E_MAIL_ACCOUNT_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_ACCOUNT_STORE, EMailAccountStoreClass))
#define E_IS_MAIL_ACCOUNT_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_ACCOUNT_STORE))
#define E_IS_MAIL_ACOCUNT_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_ACCOUNT_STORE))
#define E_MAIL_ACCOUNT_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_ACCOUNT_STORE, EMailAccountStoreClass))

G_BEGIN_DECLS

/* Avoid a circular dependency. */
struct _EMailSession;

typedef enum {
	E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE,
	E_MAIL_ACCOUNT_STORE_COLUMN_BUILTIN,
	E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED,
	E_MAIL_ACCOUNT_STORE_COLUMN_DEFAULT,
	E_MAIL_ACCOUNT_STORE_COLUMN_BACKEND_NAME,
	E_MAIL_ACCOUNT_STORE_COLUMN_DISPLAY_NAME,
	E_MAIL_ACCOUNT_STORE_COLUMN_ICON_NAME,
	E_MAIL_ACCOUNT_STORE_COLUMN_ONLINE_ACCOUNT,
	E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED_VISIBLE,
	E_MAIL_ACCOUNT_STORE_NUM_COLUMNS
} EMailAccountStoreColumn;

typedef struct _EMailAccountStore EMailAccountStore;
typedef struct _EMailAccountStoreClass EMailAccountStoreClass;
typedef struct _EMailAccountStorePrivate EMailAccountStorePrivate;

struct _EMailAccountStore {
	GtkListStore parent;
	EMailAccountStorePrivate *priv;
};

struct _EMailAccountStoreClass {
	GtkListStoreClass parent_class;

	/* Signals */
	void		(*service_added)	(EMailAccountStore *store,
						 CamelService *service);
	void		(*service_removed)	(EMailAccountStore *store,
						 CamelService *service);
	void		(*service_enabled)	(EMailAccountStore *store,
						 CamelService *service);
	void		(*service_disabled)	(EMailAccountStore *store,
						 CamelService *service);
	void		(*services_reordered)	(EMailAccountStore *store,
						 gboolean default_restored);

	/* These signals are for confirmation dialogs.
	 * Signal handler should return FALSE to abort. */
	gboolean	(*remove_requested)	(EMailAccountStore *store,
						 GtkWindow *parent_window,
						 CamelService *service);
	gboolean	(*enable_requested)	(EMailAccountStore *store,
						 GtkWindow *parent_window,
						 CamelService *service);
	gboolean	(*disable_requested)	(EMailAccountStore *store,
						 GtkWindow *parent_window,
						 CamelService *service);
};

GType		e_mail_account_store_get_type	(void) G_GNUC_CONST;
EMailAccountStore *
		e_mail_account_store_new	(struct _EMailSession *session);
void		e_mail_account_store_clear	(EMailAccountStore *store);
gboolean	e_mail_account_store_get_busy	(EMailAccountStore *store);
struct _EMailSession *
		e_mail_account_store_get_session
						(EMailAccountStore *store);
CamelService *	e_mail_account_store_get_default_service
						(EMailAccountStore *store);
void		e_mail_account_store_set_default_service
						(EMailAccountStore *store,
						 CamelService *service);
void		e_mail_account_store_add_service
						(EMailAccountStore *store,
						 CamelService *service);
void		e_mail_account_store_remove_service
						(EMailAccountStore *store,
						 GtkWindow *parent_window,
						 CamelService *service);
void		e_mail_account_store_enable_service
						(EMailAccountStore *store,
						 GtkWindow *parent_window,
						 CamelService *service);
void		e_mail_account_store_disable_service
						(EMailAccountStore *store,
						 GtkWindow *parent_window,
						 CamelService *service);
void		e_mail_account_store_queue_services
						(EMailAccountStore *store,
						 GQueue *out_queue);
void		e_mail_account_store_queue_enabled_services
						(EMailAccountStore *store,
						 GQueue *out_queue);
gboolean	e_mail_account_store_have_enabled_service
						(EMailAccountStore *store,
						 GType service_type);
void		e_mail_account_store_reorder_services
						(EMailAccountStore *store,
						 GQueue *ordered_services);
gint		e_mail_account_store_compare_services
						(EMailAccountStore *store,
						 CamelService *service_a,
						 CamelService *service_b);
gboolean	e_mail_account_store_load_sort_order
						(EMailAccountStore *store,
						 GError **error);
gboolean	e_mail_account_store_save_sort_order
						(EMailAccountStore *store,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_ACCOUNT_STORE_H */
