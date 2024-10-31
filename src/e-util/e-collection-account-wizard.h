/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_COLLECTION_ACCOUNT_WIZARD_H
#define E_COLLECTION_ACCOUNT_WIZARD_H

#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_COLLECTION_ACCOUNT_WIZARD \
	(e_collection_account_wizard_get_type ())
#define E_COLLECTION_ACCOUNT_WIZARD(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COLLECTION_ACCOUNT_WIZARD, ECollectionAccountWizard))
#define E_COLLECTION_ACCOUNT_WIZARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COLLECTION_ACCOUNT_WIZARD, ECollectionAccountWizardClass))
#define E_IS_COLLECTION_ACCOUNT_WIZARD(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COLLECTION_ACCOUNT_WIZARD))
#define E_IS_COLLECTION_ACCOUNT_WIZARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COLLECTION_ACCOUNT_WIZARD))
#define E_COLLECTION_ACCOUNT_WIZARD_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COLLECTION_ACCOUNT_WIZARD, ECollectionAccountWizardClass))

G_BEGIN_DECLS

typedef struct _ECollectionAccountWizard ECollectionAccountWizard;
typedef struct _ECollectionAccountWizardClass ECollectionAccountWizardClass;
typedef struct _ECollectionAccountWizardPrivate ECollectionAccountWizardPrivate;

/**
 * ECollectionAccountWizard:
 *
 * Contains only private data that should be read and manipulated using
 * the functions below.
 *
 * Since: 3.28
 **/
struct _ECollectionAccountWizard {
	/*< private >*/
	GtkNotebook parent;
	ECollectionAccountWizardPrivate *priv;
};

struct _ECollectionAccountWizardClass {
	/*< private >*/
	GtkNotebookClass parent_class;

	/* Signals */
	void		(* done)		(ECollectionAccountWizard *wizard,
						 const gchar *uid);
};

GType		e_collection_account_wizard_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_collection_account_wizard_new			(ESourceRegistry *registry);
GtkWindow *	e_collection_account_wizard_new_window		(GtkWindow *parent,
								 ESourceRegistry *registry);
ESourceRegistry *
		e_collection_account_wizard_get_registry	(ECollectionAccountWizard *wizard);
gboolean	e_collection_account_wizard_get_can_run		(ECollectionAccountWizard *wizard);
void		e_collection_account_wizard_reset		(ECollectionAccountWizard *wizard);
gboolean	e_collection_account_wizard_next		(ECollectionAccountWizard *wizard);
gboolean	e_collection_account_wizard_prev		(ECollectionAccountWizard *wizard);
gboolean	e_collection_account_wizard_is_finish_page	(ECollectionAccountWizard *wizard);
void		e_collection_account_wizard_run			(ECollectionAccountWizard *wizard,
								 GAsyncReadyCallback callback,
								 gpointer user_data);
void		e_collection_account_wizard_run_finish		(ECollectionAccountWizard *wizard,
								 GAsyncResult *result);
void		e_collection_account_wizard_abort		(ECollectionAccountWizard *wizard);

G_END_DECLS

#endif /* E_COLLECTION_ACCOUNT_WIZARD_H */
