/*
 * e-mail-account-manager.h
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

#ifndef E_MAIL_ACCOUNT_MANAGER_H
#define E_MAIL_ACCOUNT_MANAGER_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>
#include <mail/e-mail-account-store.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_ACCOUNT_MANAGER \
	(e_mail_account_manager_get_type ())
#define E_MAIL_ACCOUNT_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_ACCOUNT_MANAGER, EMailAccountManager))
#define E_MAIL_ACCOUNT_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_ACCOUNT_MANAGER, EMailAccountManagerClass))
#define E_IS_MAIL_ACCOUNT_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_ACCOUNT_MANAGER))
#define E_IS_MAIL_ACCOUNT_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_INSTANCE_CLASS \
	((cls), E_TYPE_MAIL_ACCOUNT_MANAGER))
#define E_MAIL_ACCOUNT_MANAGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_ACCOUNT_MANAGER, EMailAccountManagerClass))

G_BEGIN_DECLS

typedef struct _EMailAccountManager EMailAccountManager;
typedef struct _EMailAccountManagerClass EMailAccountManagerClass;
typedef struct _EMailAccountManagerPrivate EMailAccountManagerPrivate;

struct _EMailAccountManager {
	GtkGrid parent;
	EMailAccountManagerPrivate *priv;
};

struct _EMailAccountManagerClass {
	GtkGridClass parent_class;

	/* Signals */
	void		(*add_account)		(EMailAccountManager *manager);
	void		(*edit_account)		(EMailAccountManager *manager,
						 ESource *source);
};

GType		e_mail_account_manager_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_mail_account_manager_new	(EMailAccountStore *store);
EMailAccountStore *
		e_mail_account_manager_get_store
						(EMailAccountManager *manager);
void		e_mail_account_manager_add_account
						(EMailAccountManager *manager);
void		e_mail_account_manager_edit_account
						(EMailAccountManager *manager,
						 ESource *source);

G_END_DECLS

#endif /* E_MAIL_ACCOUNT_MANAGER_H */
