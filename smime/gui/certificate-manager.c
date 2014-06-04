/*
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
 *
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <glib/gi18n.h>

#include "ca-trust-dialog.h"
#include "cert-trust-dialog.h"
#include "certificate-manager.h"

#include "e-cert.h"
#include "e-cert-trust.h"
#include "e-cert-db.h"

#include "nss.h"
#include <cms.h>
#include <cert.h>
#include <certdb.h>
#include <pkcs11.h>
#include <pk11func.h>

/* XXX Hack to disable p11-kit's pkcs11.h header, since
 *     NSS headers supply the same PKCS #11 definitions. */
#define PKCS11_H 1

/* XXX Yeah, yeah... */
#define GCR_API_SUBJECT_TO_CHANGE

#include <gcr/gcr.h>

#include "shell/e-shell.h"

#define E_CERT_MANAGER_CONFIG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CERT_MANAGER_CONFIG, ECertManagerConfigPrivate))

G_DEFINE_TYPE (ECertManagerConfig, e_cert_manager_config, GTK_TYPE_BOX);

enum {
	PROP_0,
	PROP_PREFERENCES_WINDOW
};

#define ECMC_TREE_VIEW(o) ecmc->priv->o->treeview
#define PAGE_TREE_VIEW(o) o->treeview

typedef struct {
	GType type;
	const gchar *column_title;
	const gchar * (*get_cert_data_func) (ECert *cert);  /* Prototype to e_cert_get_ * functions */
	gboolean visible;				   /* Default visibility of column */
} CertTreeColumn;

static CertTreeColumn yourcerts_columns[] = {

	{ G_TYPE_STRING, N_("Certificate Name"),		e_cert_get_cn,			TRUE },
	{ G_TYPE_STRING, N_("Issued To Organization"),		e_cert_get_org,			FALSE },
	{ G_TYPE_STRING, N_("Issued To Organizational Unit"),	e_cert_get_org_unit,		FALSE },
	{ G_TYPE_STRING, N_("Serial Number"),			e_cert_get_serial_number,	TRUE },
	{ G_TYPE_STRING, N_("Purposes"),			e_cert_get_usage,		TRUE },
	{ G_TYPE_STRING, N_("Issued By"),			e_cert_get_issuer_cn,		TRUE },
	{ G_TYPE_STRING, N_("Issued By Organization"),		e_cert_get_issuer_org,		FALSE },
	{ G_TYPE_STRING, N_("Issued By Organizational Unit"),	e_cert_get_issuer_org_unit,	FALSE },
	{ G_TYPE_STRING, N_("Issued"),				e_cert_get_issued_on,		FALSE },
	{ G_TYPE_STRING, N_("Expires"),				e_cert_get_expires_on,		TRUE },
	{ G_TYPE_STRING, N_("SHA1 Fingerprint"),		e_cert_get_sha1_fingerprint,	FALSE },
	{ G_TYPE_STRING, N_("MD5 Fingerprint"),			e_cert_get_md5_fingerprint,	FALSE },
	{ G_TYPE_OBJECT, NULL,					NULL,				FALSE } /* Hidden column for ECert * object */

};
static const gchar * yourcerts_mime_types[] = { "application/x-x509-user-cert", "application/x-pkcs12", NULL };

static CertTreeColumn contactcerts_columns[] = {

	{ G_TYPE_STRING, N_("Certificate Name"),		e_cert_get_cn,			TRUE },
	{ G_TYPE_STRING, N_("Email Address"),			e_cert_get_email,		TRUE },
	{ G_TYPE_STRING, N_("Issued To Organization"),		e_cert_get_org,			FALSE },
	{ G_TYPE_STRING, N_("Issued To Organizational Unit"),	e_cert_get_org_unit,		FALSE },
	{ G_TYPE_STRING, N_("Serial Number"),			e_cert_get_serial_number,	TRUE },
	{ G_TYPE_STRING, N_("Purposes"),			e_cert_get_usage,		TRUE },
	{ G_TYPE_STRING, N_("Issued By"),			e_cert_get_issuer_cn,		TRUE },
	{ G_TYPE_STRING, N_("Issued By Organization"),		e_cert_get_issuer_org,		FALSE },
	{ G_TYPE_STRING, N_("Issued By Organizational Unit"),	e_cert_get_issuer_org_unit,	FALSE },
	{ G_TYPE_STRING, N_("Issued"),				e_cert_get_issued_on,		FALSE },
	{ G_TYPE_STRING, N_("Expires"),				e_cert_get_expires_on,		TRUE },
	{ G_TYPE_STRING, N_("SHA1 Fingerprint"),		e_cert_get_sha1_fingerprint,	FALSE },
	{ G_TYPE_STRING, N_("MD5 Fingerprint"),			e_cert_get_md5_fingerprint,	FALSE },
	{ G_TYPE_OBJECT, NULL,					NULL,				FALSE }

};
static const gchar * contactcerts_mime_types[] = { "application/x-x509-email-cert", "application/x-x509-ca-cert", NULL };

static CertTreeColumn authoritycerts_columns[] = {

	{ G_TYPE_STRING, N_("Certificate Name"),		e_cert_get_cn,			TRUE },
	{ G_TYPE_STRING, N_("Email Address"),			e_cert_get_email,		TRUE },
	{ G_TYPE_STRING, N_("Serial Number"),			e_cert_get_serial_number,	TRUE },
	{ G_TYPE_STRING, N_("Purposes"),			e_cert_get_usage,		TRUE },
	{ G_TYPE_STRING, N_("Issued By"),			e_cert_get_issuer_cn,		FALSE },
	{ G_TYPE_STRING, N_("Issued By Organization"),		e_cert_get_issuer_org,		FALSE },
	{ G_TYPE_STRING, N_("Issued By Organizational Unit"),	e_cert_get_issuer_org_unit,	FALSE },
	{ G_TYPE_STRING, N_("Issued"),				e_cert_get_issued_on,		FALSE },
	{ G_TYPE_STRING, N_("Expires"),				e_cert_get_expires_on,		TRUE },
	{ G_TYPE_STRING, N_("SHA1 Fingerprint"),		e_cert_get_sha1_fingerprint,	FALSE },
	{ G_TYPE_STRING, N_("MD5 Fingerprint"),			e_cert_get_md5_fingerprint,	FALSE },
	{ G_TYPE_OBJECT, NULL,					NULL,				FALSE }

};
static const gchar * authoritycerts_mime_types[] =  { "application/x-x509-ca-cert", NULL };

typedef struct {
	GtkTreeView *treeview;
	GtkTreeModel *streemodel;
	GHashTable *root_hash;
	GtkMenu *popup_menu;
	GtkWidget *view_button;
	GtkWidget *edit_button;
	GtkWidget *backup_button;
	GtkWidget *backup_all_button;
	GtkWidget *import_button;
	GtkWidget *delete_button;

	CertTreeColumn *columns;
	gint columns_count;

	ECertType cert_type;
	const gchar *cert_filter_name;
	const gchar **cert_mime_types;
} CertPage;

struct _ECertManagerConfigPrivate {
	GtkBuilder *builder;

	EPreferencesWindow *pref_window;

	CertPage *yourcerts_page;
	CertPage *contactcerts_page;
	CertPage *authoritycerts_page;
};

static void view_cert (GtkWidget *button, CertPage *cp);
static void edit_cert (GtkWidget *button, CertPage *cp);
static void delete_cert (GtkWidget *button, CertPage *cp);
static void import_cert (GtkWidget *button, CertPage *cp);

static void load_certs (CertPage *cp);
static void unload_certs (CertPage *cp);

static void
save_treeview_state (GtkTreeView *treeview)
{
	GKeyFile *keyfile;
	GtkTreeModel *model;
	GtkTreeSortable *sortable;
	GtkSortType sort_type;
	gint columns_count;
	gint i = 0;
	gint *list;
	gchar *cfg_file, *data;
	const gchar *tree_name;

	g_return_if_fail (treeview && GTK_IS_TREE_VIEW (treeview));

	model = gtk_tree_view_get_model (treeview);
	g_return_if_fail (model && GTK_IS_TREE_MODEL_SORT (model));

	keyfile = g_key_file_new ();
	cfg_file = g_build_filename (e_get_user_config_dir (), "cert_trees.ini", NULL);
	g_key_file_load_from_file (keyfile, cfg_file, 0, NULL);

	tree_name = gtk_widget_get_name (GTK_WIDGET (treeview));
	sortable = GTK_TREE_SORTABLE (model);

	columns_count = gtk_tree_model_get_n_columns (model) - 1; /* Ignore the last column - the ECert * holder */
	list = g_new0 (gint, columns_count);

	for (i = 0; i < columns_count; i++) {
		GtkTreeViewColumn *column = gtk_tree_view_get_column (treeview, i);
		if (gtk_tree_view_column_get_visible (column)) {
			list[gtk_tree_view_column_get_sort_column_id (column)] = gtk_tree_view_column_get_width (column);
		} else {
			list[gtk_tree_view_column_get_sort_column_id (column)] = 0;
		}
	}
	g_key_file_set_integer_list (keyfile, tree_name, "columns", list, columns_count);
	g_free (list);

	list = g_new0 (gint, columns_count);
	for (i = 0; i < columns_count; i++) {
		GtkTreeViewColumn *column = gtk_tree_view_get_column (treeview, i);
		list[i] = gtk_tree_view_column_get_sort_column_id (column);
	}
	g_key_file_set_integer_list (keyfile, tree_name, "columns-order", list, columns_count);
	g_free (list);

	gtk_tree_sortable_get_sort_column_id (sortable, &i, &sort_type);
	g_key_file_set_integer (keyfile, tree_name, "sort-column", i);

	g_key_file_set_integer (keyfile, tree_name, "sort-order", sort_type);

	data = g_key_file_to_data (keyfile, NULL, NULL);
	g_file_set_contents (cfg_file, data, -1,  NULL);

	g_free (data);
	g_free (cfg_file);
	g_key_file_free (keyfile);
}

static void
load_treeview_state (GtkTreeView *treeview)
{
	GKeyFile *keyfile;
	gint i, *list;
	gsize length;
	GtkTreeSortable *sortable;
	GtkTreeModel *model;
	gchar *cfg_file;
	const gchar *tree_name;

	g_return_if_fail (treeview && GTK_IS_TREE_VIEW (treeview));

	keyfile = g_key_file_new ();
	cfg_file = g_build_filename (e_get_user_config_dir (), "cert_trees.ini", NULL);

	if (!g_key_file_load_from_file (keyfile, cfg_file, 0, NULL)) {
		g_key_file_free (keyfile);
		g_free (cfg_file);
		return;
	}

	model = GTK_TREE_MODEL (gtk_tree_view_get_model (treeview));
	tree_name = gtk_widget_get_name (GTK_WIDGET (treeview));
	list = g_key_file_get_integer_list (keyfile, tree_name,	"columns", &length, NULL);

	if (list) {
		gboolean all_hidden = TRUE;

		if (length != (gtk_tree_model_get_n_columns (model) - 1)) {
			g_debug ("%s: Unexpected number of columns in config file", G_STRFUNC);
			g_free (list);
			goto exit;
		}

		for (i = 0; all_hidden && i < length; i++) {
			all_hidden = list[i] == 0;
		}

		for (i = 0; !all_hidden && i < length; i++) {
			GtkTreeViewColumn *column = gtk_tree_view_get_column (treeview, i);
			if (list[i]) {
				gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
				gtk_tree_view_column_set_fixed_width (column, list[i]);
				gtk_tree_view_column_set_visible (column, TRUE);
			} else {
				gtk_tree_view_column_set_visible (column, FALSE);
			}
		}
		g_free (list);
	}

	list = g_key_file_get_integer_list (keyfile, tree_name, "columns-order", &length, NULL);

	if (list) {
		GList *columns = gtk_tree_view_get_columns (treeview);

		if (length != g_list_length (columns)) {
			g_debug ("%s: Unexpected number of columns in config file", G_STRFUNC);
			g_free (list);
			goto exit;
		}

		for (i = (length - 1); i >= 0; i--) {
			if ((list[i] >= 0) && (list[i] < length)) {
				GtkTreeViewColumn *column = g_list_nth (columns, list[i])->data;
				gtk_tree_view_move_column_after (treeview, column, NULL);
			} else {
				g_warning ("%s: Invalid column number", G_STRFUNC);
			}
		}
		g_free (list);
		g_list_free (columns);
	}

	sortable = GTK_TREE_SORTABLE (gtk_tree_view_get_model (treeview));
	gtk_tree_sortable_set_sort_column_id (
		sortable,
		g_key_file_get_integer (keyfile, tree_name, "sort-column", 0),
		g_key_file_get_integer (keyfile, tree_name, "sort-order", GTK_SORT_ASCENDING));

 exit:
	g_free (cfg_file);
	g_key_file_free (keyfile);
}

static void
report_and_free_error (CertPage *cp,
                       const gchar *where,
                       GError *error)
{
	g_return_if_fail (cp != NULL);

	e_notice (
		gtk_widget_get_toplevel (GTK_WIDGET (cp->treeview)),
		GTK_MESSAGE_ERROR, "%s: %s", where,
		error ? error->message : _("Unknown error"));

	if (error != NULL)
		g_error_free (error);
}

static gboolean
treeview_header_clicked (GtkWidget *widget,
                         GdkEvent *button_event,
                         gpointer user_data)
{
	GtkMenu *menu = user_data;
	guint event_button = 0;
	guint32 event_time;

	gdk_event_get_button (button_event, &event_button);
	event_time = gdk_event_get_time (button_event);

	if (event_button != 3)
		return FALSE;

	gtk_widget_show_all (GTK_WIDGET (menu));
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event_button, event_time);

	return TRUE;
}

static void
header_popup_item_toggled (GtkCheckMenuItem *item,
                           gpointer user_data)
{
	GtkTreeViewColumn *column = user_data;

	gtk_tree_view_column_set_visible (
		column,
		gtk_check_menu_item_get_active (item));
}

static void
treeview_column_visibility_changed (GtkTreeViewColumn *column,
                                    GParamSpec *pspec,
                                    gpointer user_data)
{
	GtkCheckMenuItem *menu_item = user_data;

	gtk_check_menu_item_set_active (
		menu_item,
		gtk_tree_view_column_get_visible (column));

}

static void
treeview_selection_changed (GtkTreeSelection *selection,
                            CertPage *cp)
{
	GtkTreeIter iter;
	gboolean cert_selected = FALSE;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		ECert *cert;

		gtk_tree_model_get (
			model, &iter,
			cp->columns_count - 1, &cert,
			-1);

		if (cert) {
			cert_selected = TRUE;
			g_object_unref (cert);
		}
	}

	if (cp->delete_button)
		gtk_widget_set_sensitive (cp->delete_button, cert_selected);
	if (cp->edit_button)
		gtk_widget_set_sensitive (cp->edit_button, cert_selected);
	if (cp->view_button)
		gtk_widget_set_sensitive (cp->view_button, cert_selected);
}

static void
treeview_add_column (CertPage *cp,
                     gint column_index)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkWidget *header, *item;

	if (cp->columns[column_index].type != G_TYPE_STRING)
		return;

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	column = gtk_tree_view_column_new_with_attributes (
		gettext (cp->columns[column_index].column_title),
		cell, "text", column_index, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, column_index);
	gtk_tree_view_column_set_visible (column, cp->columns[column_index].visible);
	gtk_tree_view_append_column (cp->treeview, column);

	header = gtk_tree_view_column_get_button (column);
	g_signal_connect (
		header, "button-release-event",
		G_CALLBACK (treeview_header_clicked), cp->popup_menu);

	/* The first column should not be concealable so there's no point in displaying
	 * it in the popup menu */
	if (column_index == 0)
		return;

	/* Add item to header popup */
	item = gtk_check_menu_item_new_with_label (
		gettext (cp->columns[column_index].column_title));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), cp->columns[column_index].visible);
	gtk_menu_attach (cp->popup_menu, item, 0, 1, column_index - 1, column_index);
	g_signal_connect (
		item, "toggled",
		G_CALLBACK (header_popup_item_toggled), column);
	e_signal_connect_notify (
		column, "notify::visible",
		G_CALLBACK (treeview_column_visibility_changed), item);
}

struct find_cert_data {
	ECert *cert;
	GtkTreePath *path;
	CertPage *cp;
};

static gboolean
find_cert_cb (GtkTreeModel *model,
              GtkTreePath *path,
              GtkTreeIter *iter,
              gpointer data)
{
	struct find_cert_data *fcd = data;
	ECert *cert = NULL;

	g_return_val_if_fail (model != NULL, TRUE);
	g_return_val_if_fail (iter != NULL, TRUE);
	g_return_val_if_fail (data != NULL, TRUE);

	/* Get the certificate object from model */
	gtk_tree_model_get (model, iter, (fcd->cp->columns_count - 1), &cert, -1);

	if (cert && g_strcmp0 (e_cert_get_serial_number (cert), e_cert_get_serial_number (fcd->cert)) == 0
	    && g_strcmp0 (e_cert_get_subject_name (cert), e_cert_get_subject_name (fcd->cert)) == 0
	    && g_strcmp0 (e_cert_get_sha1_fingerprint (cert), e_cert_get_sha1_fingerprint (fcd->cert)) == 0
	    && g_strcmp0 (e_cert_get_md5_fingerprint (cert), e_cert_get_md5_fingerprint (fcd->cert)) == 0) {
		fcd->path = gtk_tree_path_copy (path);
	}

	if (cert)
		g_object_unref (cert);

	return fcd->path != NULL;
}

static void
select_certificate (CertPage *cp,
                    ECert *cert)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	struct find_cert_data fcd;

	g_return_if_fail (cp != NULL);
	g_return_if_fail (cert != NULL);
	g_return_if_fail (E_IS_CERT (cert));

	model = gtk_tree_view_get_model (cp->treeview);
	g_return_if_fail (model != NULL);

	fcd.cp = cp;
	fcd.cert = cert;
	fcd.path = NULL;

	gtk_tree_model_foreach (model, find_cert_cb, &fcd);

	if (fcd.path) {
		gtk_tree_view_expand_to_path (cp->treeview, fcd.path);

		selection = gtk_tree_view_get_selection (cp->treeview);
		gtk_tree_selection_select_path (selection, fcd.path);

		gtk_tree_view_scroll_to_cell (cp->treeview, fcd.path, NULL, TRUE, 0.5, 0.5);
		gtk_tree_path_free (fcd.path);
	}
}

static void
view_cert (GtkWidget *button,
           CertPage *cp)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (cp->treeview), NULL, &iter)) {
		ECert *cert;

		gtk_tree_model_get (
			GTK_TREE_MODEL (cp->streemodel), &iter,
			cp->columns_count - 1, &cert,
			-1);

		if (cert) {
			GtkWidget *dialog, *parent;

			parent = gtk_widget_get_toplevel (button);
			if (!parent || !GTK_IS_WINDOW (parent))
				parent = NULL;

			dialog = e_cert_manager_new_certificate_viewer ((GtkWindow *) parent, cert);
			g_signal_connect (
				dialog, "response",
				G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_widget_show (dialog);
			g_object_unref (cert);
		}
	}
}

static void
edit_cert (GtkWidget *button,
           CertPage *cp)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (cp->treeview), NULL, &iter)) {
		ECert *cert;

		gtk_tree_model_get (
			GTK_TREE_MODEL (cp->streemodel), &iter,
			cp->columns_count - 1, &cert,
			-1);

		if (cert) {
			GtkWidget *dialog;
			CERTCertificate *icert = e_cert_get_internal_cert (cert);

			switch (cp->cert_type) {
				case E_CERT_CA:
					dialog = ca_trust_dialog_show (cert, FALSE);
					ca_trust_dialog_set_trust (
						dialog,
						e_cert_trust_has_trusted_ca (icert->trust, TRUE,  FALSE, FALSE),
						e_cert_trust_has_trusted_ca (icert->trust, FALSE, TRUE,  FALSE),
						e_cert_trust_has_trusted_ca (icert->trust, FALSE, FALSE, TRUE));
					break;
				case E_CERT_CONTACT:
					dialog = cert_trust_dialog_show (cert);
					break;
				default:
					/* Other cert types cannot be edited */
					return;
			}

			if ((gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) &&
			    (cp->cert_type == E_CERT_CA)) {
				gboolean trust_ssl, trust_email, trust_objsign;
				CERTCertTrust trust;

				ca_trust_dialog_get_trust (
					dialog,
					&trust_ssl, &trust_email, &trust_objsign);

				e_cert_trust_init (&trust);
				e_cert_trust_set_valid_ca (&trust);
				e_cert_trust_add_ca_trust (
					&trust,
					trust_ssl, trust_email, trust_objsign);

				e_cert_db_change_cert_trust (icert, &trust);
			}

			gtk_widget_destroy (dialog);
			g_object_unref (cert);
		}
	}
}

static void
import_cert (GtkWidget *button,
             CertPage *cp)
{
	GtkWidget *filesel;
	GtkFileFilter *filter;
	gint i;

	filesel = gtk_file_chooser_dialog_new (
		_("Select a certificate to import..."), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Open"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (filesel), GTK_RESPONSE_OK);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, cp->cert_filter_name);
	for (i = 0; cp->cert_mime_types[i] != NULL; i++) {
		gtk_file_filter_add_mime_type (filter, cp->cert_mime_types[i]);
	}
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (filesel), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (filesel), filter);

	if (gtk_dialog_run (GTK_DIALOG (filesel)) == GTK_RESPONSE_OK) {
		gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel));
		GSList *imported_certs = NULL;
		GError *error = NULL;
		gboolean import;

		/* destroy dialog to get rid of it in the GUI */
		gtk_widget_destroy (filesel);

		switch (cp->cert_type) {
			case E_CERT_USER:
				import = e_cert_db_import_pkcs12_file (e_cert_db_peek (), filename, &error);
				break;
			case E_CERT_CONTACT:
			case E_CERT_CA:
				import = e_cert_db_import_certs_from_file (
					e_cert_db_peek (), filename,
					cp->cert_type, &imported_certs, &error);
				break;
			default:
				g_free (filename);
				return;
		}

		if (import) {
			unload_certs (cp);
			load_certs (cp);

			if (imported_certs)
				select_certificate (cp, imported_certs->data);

		} else {
			report_and_free_error (cp, _("Failed to import certificate"), error);
		}

		g_slist_foreach (imported_certs, (GFunc) g_object_unref, NULL);
		g_slist_free (imported_certs);
		g_free (filename);
	} else
		gtk_widget_destroy (filesel);
}

static void
delete_cert (GtkWidget *button,
             CertPage *cp)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (cp->treeview), NULL, &iter)) {
		ECert *cert;

		gtk_tree_model_get (
			GTK_TREE_MODEL (cp->streemodel), &iter,
			cp->columns_count - 1, &cert,
			-1);

		if (cert && e_cert_db_delete_cert (e_cert_db_peek (), cert)) {
			GtkTreeIter child_iter, parent_iter;
			gboolean has_parent;
			GtkTreeStore *store = GTK_TREE_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (cp->streemodel)));

			gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (cp->streemodel), &child_iter, &iter);
			has_parent = gtk_tree_model_iter_parent (GTK_TREE_MODEL (store), &parent_iter, &child_iter);
			gtk_tree_store_remove (store, &child_iter);

			/* Remove parent if it became empty */
			if (has_parent && gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), &parent_iter) == 0)
				gtk_tree_store_remove (store, &parent_iter);

			/* we need two unrefs here, one to unref the
			 * gtk_tree_model_get above, and one to unref
			 * the initial ref when we created the cert
			 * and added it to the tree */
			g_object_unref (cert);
			g_object_unref (cert);
		} else if (cert) {
			g_object_unref (cert);
		}
	}

}

static void
add_cert (CertPage *cp,
          ECert *cert)
{
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;
	const gchar *organization = e_cert_get_org (cert);
	GtkTreeModel *model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (cp->streemodel));
	gint i;

	if (organization) {
		parent_iter = g_hash_table_lookup (cp->root_hash, organization);
		if (!parent_iter) {
			/* create a new toplevel node */
			gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

			gtk_tree_store_set (
				GTK_TREE_STORE (model), &iter,
				0, organization, -1);

			/* now copy it off into parent_iter and insert it into
			 * the hashtable */
			parent_iter = gtk_tree_iter_copy (&iter);
			g_hash_table_insert (cp->root_hash, g_strdup (organization), parent_iter);
		}
	}

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent_iter);

	for (i = 0; i < cp->columns_count; i++) {
		const gchar * (*get_cert_data_func) (ECert *cert);

		/* When e_cert_get_cn() is empty, use _get_nickname() */
		if ((cp->columns[i].get_cert_data_func == e_cert_get_cn) && (!e_cert_get_cn (cert))) {
			get_cert_data_func = e_cert_get_nickname;
		} else {
			get_cert_data_func = cp->columns[i].get_cert_data_func;
		}

		if (cp->columns[i].type == G_TYPE_STRING) {
			gtk_tree_store_set (
				GTK_TREE_STORE (model), &iter,
				i, get_cert_data_func (cert), -1);
		} else if (cp->columns[i].type == G_TYPE_OBJECT) {
			gtk_tree_store_set (
				GTK_TREE_STORE (model), &iter,
				i, cert, -1);
		}
	}
}

static void
unload_certs (CertPage *cp)
{
	GtkTreeStore *treemodel;
	GType types[cp->columns_count];
	gint i;

	g_return_if_fail (cp != NULL);

	for (i = 0; i < cp->columns_count; i++)
		types[i] = cp->columns[i].type;
	treemodel = gtk_tree_store_newv (cp->columns_count, types);

	if (cp->streemodel)
		g_object_unref (cp->streemodel);

	cp->streemodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (treemodel));

	g_object_unref (treemodel);
	gtk_tree_view_set_model (cp->treeview, cp->streemodel);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (cp->streemodel), 0, GTK_SORT_ASCENDING);

	if (cp->root_hash)
		g_hash_table_destroy (cp->root_hash);

	cp->root_hash = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_iter_free);
}

static void
load_certs (CertPage *cp)
{
	CERTCertList *certList;
	CERTCertListNode *node;

	g_return_if_fail (cp != NULL);

	certList = PK11_ListCerts (PK11CertListUnique, NULL);

	for (node = CERT_LIST_HEAD (certList);
	     !CERT_LIST_END (node, certList);
	     node = CERT_LIST_NEXT (node)) {
		ECert *cert = e_cert_new (CERT_DupCertificate ((CERTCertificate *) node->cert));
		ECertType ct = e_cert_get_cert_type (cert);

		/* show everything else in a contact tab */
		if (ct == cp->cert_type || (cp->cert_type == E_CERT_CONTACT && ct != E_CERT_CA && ct != E_CERT_USER)) {
			add_cert (cp, cert);
		} else {
			g_object_unref (cert);
		}
	}

	CERT_DestroyCertList (certList);
}

static gboolean
populate_ui (ECertManagerConfig *ecmc)
{
	/* This is an idle callback. */

	ECertManagerConfigPrivate *priv = ecmc->priv;

	unload_certs (priv->yourcerts_page);
	load_certs (priv->yourcerts_page);

	unload_certs (priv->contactcerts_page);
	load_certs (priv->contactcerts_page);

	unload_certs (priv->authoritycerts_page);
	load_certs (priv->authoritycerts_page);

	/* expand all three trees */
	gtk_tree_view_expand_all (ECMC_TREE_VIEW (yourcerts_page));
	gtk_tree_view_expand_all (ECMC_TREE_VIEW (contactcerts_page));
	gtk_tree_view_expand_all (ECMC_TREE_VIEW (authoritycerts_page));

	/* Now load settings of each treeview */
	load_treeview_state (ECMC_TREE_VIEW (yourcerts_page));
	load_treeview_state (ECMC_TREE_VIEW (contactcerts_page));
	load_treeview_state (ECMC_TREE_VIEW (authoritycerts_page));

	return FALSE;
}

static void
initialize_ui (CertPage *cp)
{
	GtkTreeSelection *selection;
	gint i;

	cp->popup_menu = GTK_MENU (gtk_menu_new ());

	/* Add columns to treeview */
	for (i = 0; i < cp->columns_count; i++)
		treeview_add_column (cp, i);

	selection = gtk_tree_view_get_selection (cp->treeview);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (treeview_selection_changed), cp);

	if (cp->import_button)
		g_signal_connect (
			cp->import_button, "clicked",
			G_CALLBACK (import_cert), cp);

	if (cp->edit_button)
		g_signal_connect (
			cp->edit_button, "clicked",
			G_CALLBACK (edit_cert), cp);

	if (cp->delete_button)
		g_signal_connect (
			cp->delete_button, "clicked",
			G_CALLBACK (delete_cert), cp);

	if (cp->view_button)
		g_signal_connect (
			cp->view_button, "clicked",
			G_CALLBACK (view_cert), cp);
}

static void
cert_manager_config_window_hide (ECertManagerConfig *ecmc,
                                 EPreferencesWindow *epw)
{
	g_return_if_fail (ecmc);

	save_treeview_state (ECMC_TREE_VIEW (yourcerts_page));
	save_treeview_state (ECMC_TREE_VIEW (contactcerts_page));
	save_treeview_state (ECMC_TREE_VIEW (authoritycerts_page));
}

static void
free_cert (GtkTreeModel *model,
           GtkTreePath *path,
           GtkTreeIter *iter,
           gpointer user_data)
{
	CertPage *cp = user_data;
	ECert *cert;

	gtk_tree_model_get (model, iter, cp->columns_count - 1, &cert, -1);

	/* Double unref: one for gtk_tree_model_get() and one for e_cert_new() */
	g_object_unref (cert);
	g_object_unref (cert);
}

static void
cert_page_free (CertPage *cp)
{
	if (!cp)
		return;

	if (cp->streemodel) {
		gtk_tree_model_foreach (GTK_TREE_MODEL (cp->streemodel),
			(GtkTreeModelForeachFunc) free_cert, cp);
		g_object_unref (cp->streemodel);
		cp->streemodel = NULL;
	}

	if (cp->root_hash) {
		g_hash_table_unref (cp->root_hash);
		cp->root_hash = NULL;
	}

	g_free (cp);
}

static void
cert_manager_config_dispose (GObject *object)
{
	ECertManagerConfig *ecmc = E_CERT_MANAGER_CONFIG (object);

	if (ecmc->priv->yourcerts_page) {
		cert_page_free (ecmc->priv->yourcerts_page);
		ecmc->priv->yourcerts_page = NULL;
	}

	if (ecmc->priv->contactcerts_page) {
		cert_page_free (ecmc->priv->contactcerts_page);
		ecmc->priv->contactcerts_page = NULL;
	}

	if (ecmc->priv->authoritycerts_page) {
		cert_page_free (ecmc->priv->authoritycerts_page);
		ecmc->priv->authoritycerts_page = NULL;
	}

	if (ecmc->priv->builder) {
		g_object_unref (ecmc->priv->builder);
			ecmc->priv->builder = NULL;
	}

	if (ecmc->priv->pref_window) {
		g_signal_handlers_disconnect_matched (ecmc->priv->pref_window, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ecmc);
		ecmc->priv->pref_window = NULL;
	}

	G_OBJECT_CLASS (e_cert_manager_config_parent_class)->dispose (object);
}

static void
cert_manager_config_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	ECertManagerConfig *ecmc = E_CERT_MANAGER_CONFIG (object);

	switch (property_id) {
		case PROP_PREFERENCES_WINDOW:
			ecmc->priv->pref_window = g_value_get_object (value);
			/* When the preferences window is "closed" (= hidden), save
			 * state of all treeviews. */
			g_signal_connect_swapped (
				ecmc->priv->pref_window, "hide",
				G_CALLBACK (cert_manager_config_window_hide), ecmc);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_cert_manager_config_class_init (ECertManagerConfigClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ECertManagerConfigPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cert_manager_config_set_property;
	object_class->dispose = cert_manager_config_dispose;

	g_object_class_install_property (
		object_class,
		PROP_PREFERENCES_WINDOW,
		g_param_spec_object (
			"preferences-window",
			NULL,
			NULL,
			E_TYPE_PREFERENCES_WINDOW,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

static void
e_cert_manager_config_init (ECertManagerConfig *ecmc)
{
	ECertManagerConfigPrivate *priv;
	GtkWidget *parent, *widget;
	CertPage *cp;

	priv = E_CERT_MANAGER_CONFIG_GET_PRIVATE (ecmc);
	ecmc->priv = priv;

	/* We need to peek the db here to make sure it (and NSS) are fully initialized. */
	e_cert_db_peek ();

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "smime-ui.ui");

	cp = g_new0 (CertPage, 1);
	priv->yourcerts_page = cp;
	cp->treeview = GTK_TREE_VIEW (e_builder_get_widget (priv->builder, "yourcerts-treeview"));
	cp->streemodel = NULL;
	cp->view_button = e_builder_get_widget (priv->builder, "your-view-button");
	cp->backup_button = e_builder_get_widget (priv->builder, "your-backup-button");
	cp->backup_all_button = e_builder_get_widget (priv->builder, "your-backup-all-button");
	cp->edit_button = NULL;
	cp->import_button = e_builder_get_widget (priv->builder, "your-import-button");
	cp->delete_button = e_builder_get_widget (priv->builder, "your-delete-button");
	cp->columns = yourcerts_columns;
	cp->columns_count = G_N_ELEMENTS (yourcerts_columns);
	cp->cert_type = E_CERT_USER;
	cp->cert_filter_name = _("All PKCS12 files");
	cp->cert_mime_types = yourcerts_mime_types;
	initialize_ui (cp);

	cp = g_new0 (CertPage, 1);
	priv->contactcerts_page = cp;
	cp->treeview = GTK_TREE_VIEW (e_builder_get_widget (priv->builder, "contactcerts-treeview"));
	cp->streemodel = NULL;
	cp->view_button = e_builder_get_widget (priv->builder, "contact-view-button");
	cp->backup_button = NULL;
	cp->backup_all_button = NULL;
	cp->edit_button = e_builder_get_widget (priv->builder, "contact-edit-button");
	cp->import_button = e_builder_get_widget (priv->builder, "contact-import-button");
	cp->delete_button = e_builder_get_widget (priv->builder, "contact-delete-button");
	cp->columns = contactcerts_columns;
	cp->columns_count = G_N_ELEMENTS (contactcerts_columns);
	cp->cert_type = E_CERT_CONTACT;
	cp->cert_filter_name = _("All email certificate files");
	cp->cert_mime_types = contactcerts_mime_types;
	initialize_ui (cp);

	cp = g_new0 (CertPage, 1);
	priv->authoritycerts_page = cp;
	cp->treeview = GTK_TREE_VIEW (e_builder_get_widget (priv->builder, "authoritycerts-treeview"));
	cp->streemodel = NULL;
	cp->view_button = e_builder_get_widget (priv->builder, "authority-view-button");
	cp->backup_button = NULL;
	cp->backup_all_button = NULL;
	cp->edit_button = e_builder_get_widget (priv->builder, "authority-edit-button");
	cp->import_button = e_builder_get_widget (priv->builder, "authority-import-button");
	cp->delete_button = e_builder_get_widget (priv->builder, "authority-delete-button");
	cp->columns = authoritycerts_columns;
	cp->columns_count = G_N_ELEMENTS (authoritycerts_columns);
	cp->cert_type = E_CERT_CA;
	cp->cert_filter_name = _("All CA certificate files");
	cp->cert_mime_types = authoritycerts_mime_types;
	initialize_ui (cp);

	/* Run this in an idle callback so Evolution has a chance to
	 * fully initialize itself and start its main loop before we
	 * load certificates, since doing so may trigger a password
	 * dialog, and dialogs require a main loop.
	 * Schedule with priority higher than gtk+ uses for animations
	 * (check docs for G_PRIORITY_HIGH_IDLE). */
	g_idle_add_full (G_PRIORITY_DEFAULT, (GSourceFunc) populate_ui, ecmc, NULL);

	/* Disconnect cert-manager-notebook from it's window and attach it
	 * to this ECertManagerConfig */
	widget = e_builder_get_widget (priv->builder, "cert-manager-notebook");
	parent = gtk_widget_get_parent (widget);
	gtk_container_remove (GTK_CONTAINER (parent), widget);
	gtk_box_pack_start (GTK_BOX (ecmc), widget, TRUE, TRUE, 0);
	gtk_widget_show_all (widget);

	/* FIXME: remove when implemented */
	gtk_widget_set_sensitive (priv->yourcerts_page->backup_button, FALSE);
	gtk_widget_set_sensitive (priv->yourcerts_page->backup_all_button, FALSE);
}

GtkWidget *
e_cert_manager_config_new (EPreferencesWindow *window)
{
	ECertManagerConfig *ecmc;

	ecmc = g_object_new (E_TYPE_CERT_MANAGER_CONFIG, "preferences-window", window, NULL);

	return GTK_WIDGET (ecmc);
}

/* Helper for e_cert_manager_new_certificate_viewer() */
static void
cert_manager_parser_parsed_cb (GcrParser *parser,
                               GcrParsed **out_parsed)
{
	GcrParsed *parsed;

	parsed = gcr_parser_get_parsed (parser);
	g_return_if_fail (parsed != NULL);

	*out_parsed = gcr_parsed_ref (parsed);
}

GtkWidget *
e_cert_manager_new_certificate_viewer (GtkWindow *parent,
                                       ECert *cert)
{
	GcrParser *parser;
	GcrParsed *parsed = NULL;
	GcrCertificate *certificate;
	GckAttributes *attributes;
	GcrCertificateWidget *certificate_widget;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *widget;
	gchar *subject_name;
	const guchar *der_data = NULL;
	gsize der_length;
	GError *local_error = NULL;

	g_return_val_if_fail (cert != NULL, NULL);

	certificate = GCR_CERTIFICATE (cert);
	der_data = gcr_certificate_get_der_data (certificate, &der_length);

	parser = gcr_parser_new ();
	g_signal_connect (
		parser, "parsed",
		G_CALLBACK (cert_manager_parser_parsed_cb), &parsed);
	gcr_parser_parse_data (
		parser, der_data, der_length, &local_error);
	g_object_unref (parser);

	/* Sanity check. */
	g_return_val_if_fail (
		((parsed != NULL) && (local_error == NULL)) ||
		((parsed == NULL) && (local_error != NULL)), NULL);

	if (local_error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, local_error->message);
		g_clear_error (&local_error);
		return NULL;
	}

	attributes = gcr_parsed_get_attributes (parsed);
	subject_name = gcr_certificate_get_subject_name (certificate);

	dialog = gtk_dialog_new_with_buttons (
		subject_name, parent,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Close"), GTK_RESPONSE_CLOSE,
		NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	certificate_widget = gcr_certificate_widget_new (certificate);
	gcr_certificate_widget_set_attributes (certificate_widget, attributes);

	widget = GTK_WIDGET (certificate_widget);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	g_free (subject_name);
	gcr_parsed_unref (parsed);

	return dialog;
}
