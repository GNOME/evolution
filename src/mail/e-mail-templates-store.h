/*
 * SPDX-FileCopyrightText: (C) 2016 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef E_MAIL_TEMPLATES_STORE_H
#define E_MAIL_TEMPLATES_STORE_H

#include <glib.h>
#include <gtk/gtk.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>
#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>
#include <mail/e-mail-account-store.h>
#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_TEMPLATES_STORE \
	(e_mail_templates_store_get_type ())
#define E_MAIL_TEMPLATES_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_TEMPLATES_STORE, EMailTemplatesStore))
#define E_MAIL_TEMPLATES_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_TEMPLATES_STORE, EMailTemplatesStoreClass))
#define E_IS_MAIL_TEMPLATES_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_TEMPLATES_STORE))
#define E_IS_MAIL_TEMPLATES_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_TEMPLATES_STORE))
#define E_MAIL_TEMPLATES_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_TEMPLATES_STORE, EMailTemplatesStoreClass))

G_BEGIN_DECLS

enum {
	E_MAIL_TEMPLATES_STORE_COLUMN_DISPLAY_NAME = 0,	/* gchar * */
	E_MAIL_TEMPLATES_STORE_COLUMN_FOLDER,		/* CamelFolder * */
	E_MAIL_TEMPLATES_STORE_COLUMN_MESSAGE_UID,		/* gchar * */
	E_MAIL_TEMPLATES_STORE_N_COLUMNS
};

typedef struct _EMailTemplatesStore EMailTemplatesStore;
typedef struct _EMailTemplatesStoreClass EMailTemplatesStoreClass;
typedef struct _EMailTemplatesStorePrivate EMailTemplatesStorePrivate;

/**
 * EMailTemplatesStore:
 *
 * Contains only private data that should be read and manipulated using
 * the functions below.
 **/
struct _EMailTemplatesStore {
	GObject parent;
	EMailTemplatesStorePrivate *priv;
};

struct _EMailTemplatesStoreClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*changed)		(EMailTemplatesStore *templates_store);
};

typedef void	(* EMailTemplatesStoreActionFunc)	(EMailTemplatesStore *templates_store,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 gpointer user_data);

GType		e_mail_templates_store_get_type	(void) G_GNUC_CONST;
EMailTemplatesStore *
		e_mail_templates_store_ref_default
						(EMailAccountStore *account_store);
EMailAccountStore *
		e_mail_templates_store_ref_account_store
						(EMailTemplatesStore *templates_store);
void		e_mail_templates_store_update_menu
						(EMailTemplatesStore *templates_store,
						 GMenu *menu_to_update,
						 EUIManager *ui_manager,
						 EMailTemplatesStoreActionFunc action_cb,
						 gpointer action_cb_user_data);
GtkTreeStore *	e_mail_templates_store_build_model
						(EMailTemplatesStore *templates_store,
						 const gchar *find_folder_uri,
						 const gchar *find_message_uid,
						 gboolean *out_found_message,
						 GtkTreeIter *out_found_iter);

G_END_DECLS

#endif /* E_MAIL_TEMPLATES_STORE_H */
