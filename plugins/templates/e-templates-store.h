/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_TEMPLATES_STORE_H
#define E_TEMPLATES_STORE_H

#include <glib.h>
#include <gtk/gtk.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>
#include <libemail-engine/libemail-engine.h>
#include <mail/e-mail-account-store.h>
#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_TEMPLATES_STORE \
	(e_templates_store_get_type ())
#define E_TEMPLATES_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TEMPLATES_STORE, ETemplatesStore))
#define E_TEMPLATES_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TEMPLATES_STORE, ETemplatesStoreClass))
#define E_IS_TEMPLATES_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TEMPLATES_STORE))
#define E_IS_TEMPLATES_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TEMPLATES_STORE))
#define E_TEMPLATES_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TEMPLATES_STORE, ETemplatesStoreClass))

G_BEGIN_DECLS

typedef struct _ETemplatesStore ETemplatesStore;
typedef struct _ETemplatesStoreClass ETemplatesStoreClass;
typedef struct _ETemplatesStorePrivate ETemplatesStorePrivate;

/**
 * ETemplatesStore:
 *
 * Contains only private data that should be read and manipulated using
 * the functions below.
 **/
struct _ETemplatesStore {
	GObject parent;
	ETemplatesStorePrivate *priv;
};

struct _ETemplatesStoreClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*changed)		(ETemplatesStore *templates_store);
};

typedef void	(* ETemplatesStoreActionFunc)	(ETemplatesStore *templates_store,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 gpointer user_data);

GType		e_templates_store_get_type	(void) G_GNUC_CONST;
ETemplatesStore *
		e_templates_store_ref_default	(EMailAccountStore *account_store);
EMailAccountStore *
		e_templates_store_ref_account_store
						(ETemplatesStore *templates_store);
void		e_templates_store_build_menu	(ETemplatesStore *templates_store,
						 EShellView *shell_view,
						 GtkUIManager *ui_manager,
						 GtkActionGroup *action_group,
						 const gchar *base_menu_path,
						 guint merge_id,
						 ETemplatesStoreActionFunc action_cb,
						 gpointer action_cb_user_data);

G_END_DECLS

#endif /* E_TEMPLATES_STORE_H */
