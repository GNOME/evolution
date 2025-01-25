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

#include "evolution-config.h"

#include <gtk/gtk.h>

#include <glib/gi18n.h>

#include <camel/camel.h>

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

#include <libedataserverui/libedataserverui.h>

#include "shell/e-shell.h"

enum {
	PROP_0,
	PROP_PREFERENCES_WINDOW
};

#define ECMC_TREE_VIEW(o) ecmc->priv->o->treeview
#define PAGE_TREE_VIEW(o) o->treeview

#define STRING_IS_EMPTY(x)      (!(x) || !(*(x)))

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
	{ G_TYPE_STRING, N_("SHA256 Fingerprint"),		e_cert_get_sha256_fingerprint,	FALSE },
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
	{ G_TYPE_STRING, N_("SHA256 Fingerprint"),		e_cert_get_sha256_fingerprint,	FALSE },
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
	{ G_TYPE_STRING, N_("SHA256 Fingerprint"),		e_cert_get_sha256_fingerprint,	FALSE },
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

	GtkTreeModel *mail_model;
	GtkTreeView *mail_tree_view; /* not referenced */

	GCancellable *load_all_certs_cancellable;
};

typedef struct {
	GFile 	  **file;
	GtkWidget *entry1;
	GtkWidget *entry2;
	GtkWidget *match_label;
	GtkWidget *save_button;
	CertPage  *cp;
	ECert     *cert;
} BackupData;

G_DEFINE_TYPE_WITH_PRIVATE (ECertManagerConfig, e_cert_manager_config, GTK_TYPE_BOX)

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
	g_return_if_fail (model && GTK_IS_TREE_SORTABLE (model));

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
	gint sort_column, sort_order;
	GError *error = NULL;

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

	sort_column = g_key_file_get_integer (keyfile, tree_name, "sort-column", &error);
	if (error) {
		sort_column = 0;
		g_clear_error (&error);
	}

	sort_order = g_key_file_get_integer (keyfile, tree_name, "sort-order", &error);
	if (error) {
		sort_order = GTK_SORT_ASCENDING;
		g_clear_error (&error);
	}

	sortable = GTK_TREE_SORTABLE (gtk_tree_view_get_model (treeview));
	gtk_tree_sortable_set_sort_column_id (sortable, sort_column, sort_order);

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

	gdk_event_get_button (button_event, &event_button);

	if (event_button != 3)
		return FALSE;

	gtk_widget_show_all (GTK_WIDGET (menu));

	if (!gtk_menu_get_attach_widget (menu))
		gtk_menu_attach_to_widget (menu, widget, NULL);

	gtk_menu_popup_at_pointer (menu, button_event);

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
	if (cp->backup_button)
		gtk_widget_set_sensitive (cp->backup_button, cert_selected);
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
	    && g_strcmp0 (e_cert_get_sha256_fingerprint (cert), e_cert_get_sha256_fingerprint (fcd->cert)) == 0
	    && g_strcmp0 (e_cert_get_md5_fingerprint (cert), e_cert_get_md5_fingerprint (fcd->cert)) == 0) {
		fcd->path = gtk_tree_path_copy (path);
	}

	g_clear_object (&cert);

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
open_cert_viewer (GtkWidget *widget,
		  ECert *cert)
{
	GtkWidget *dialog, *parent;

	parent = gtk_widget_get_toplevel (widget);
	if (!parent || !GTK_IS_WINDOW (parent))
		parent = NULL;

	dialog = e_cert_manager_new_certificate_viewer ((GtkWindow *) parent, cert);
	g_signal_connect (
		dialog, "response",
		G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);
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
			open_cert_viewer (button, cert);
			g_object_unref (cert);
		}
	}
}

static gboolean
cert_backup_dialog_sensitize (GtkWidget *widget,
                              GdkEvent *event,
                              BackupData *data)
{

	const gchar *txt1, *txt2;
	gboolean is_sensitive = TRUE;

	txt1 = gtk_entry_get_text (GTK_ENTRY (data->entry1));
	txt2 = gtk_entry_get_text (GTK_ENTRY (data->entry2));

	if (*data->file == NULL)
		is_sensitive = FALSE;

	if (STRING_IS_EMPTY (txt1) && STRING_IS_EMPTY (txt2)) {
		gtk_widget_set_visible (data->match_label, FALSE);
		is_sensitive = FALSE;
	} else if (g_strcmp0 (txt1, txt2) == 0) {
		gtk_widget_set_visible (data->match_label, FALSE);
	} else {
		gtk_widget_set_visible (data->match_label, TRUE);
		is_sensitive = FALSE;
	}

	gtk_widget_set_sensitive (data->save_button, is_sensitive);

	return FALSE;
}

static void
cert_backup_dialog_maybe_correct_extension (GtkFileChooser *file_chooser)
{
	gchar *name = gtk_file_chooser_get_current_name (file_chooser);

	if (!g_str_has_suffix (name, ".p12")) {
		gchar *new_name = g_strconcat (name, ".p12", NULL);
		gtk_file_chooser_set_current_name (file_chooser, new_name);
		g_free (new_name);
	}

	g_free (name);
}

static void
run_cert_backup_dialog_file_chooser (GtkButton *file_button,
                                     BackupData *data)
{
	GtkFileChooserNative *native;
	GtkFileFilter *filter;
	GtkWidget *toplevel;
	gchar *filename;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (file_button));
	native = gtk_file_chooser_native_new (
		_("Select a file to backup your key and certificate…"),
		GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Save"), _("_Cancel"));
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (native), TRUE);
	/* To Translators:
	 * %s-backup.p12 is the default file name suggested by the file selection dialog,
	 * when a user wants to backup one of her/his private keys/certificates.
	 * For example: gnomedev-backup.p12
	 */
	filename = g_strdup_printf (_("%s-backup.p12"), e_cert_get_nickname (data->cert));
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (native), filename);
	g_free (filename);

	if (*data->file) {
		gtk_file_chooser_set_file (GTK_FILE_CHOOSER (native), *data->file, NULL);
	}

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, data->cp->cert_filter_name);
	gtk_file_filter_add_mime_type (filter, "application/x-pkcs12");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		gchar *basename;

		cert_backup_dialog_maybe_correct_extension (GTK_FILE_CHOOSER (native));

		g_clear_object (&(*data->file));
		*data->file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));

		basename = g_file_get_basename (*data->file);
		gtk_button_set_label (file_button, basename);
		g_free (basename);
	}

	/* destroy dialog to get rid of it in the GUI */
	g_object_unref (native);

	cert_backup_dialog_sensitize (GTK_WIDGET (file_button), NULL, data);
	gtk_widget_grab_focus (GTK_WIDGET (data->entry1));
}

static gint
run_cert_backup_dialog (CertPage *cp,
			GtkWidget *parent,
                        ECert *cert,
                        GFile **file,
                        gchar **password,
                        gboolean *save_chain)
{
	gint row = 0, col = 0, response;
	const gchar *format = "<span foreground=\"red\">%s</span>";
	gchar *markup;
	GtkWidget *dialog, *content_area, *button, *label, *chain;
	GtkGrid *grid;
	GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
	BackupData data;

	data.cp = cp;
	data.cert = cert;
	data.file = file;

	dialog = gtk_dialog_new_with_buttons (
		_("Backup Certificate"),
		GTK_IS_WINDOW (parent) ? GTK_WINDOW (parent) : NULL, flags,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Save"), GTK_RESPONSE_OK,
		NULL);
	g_object_set (dialog, "resizable", FALSE, NULL);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	g_object_set (content_area, "border-width", 6, NULL);

	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (grid, "column-spacing", 12, NULL);
	g_object_set (grid, "row-spacing", 6, NULL);

	/* filename selection */
	label = gtk_label_new_with_mnemonic (_("_File name:"));
	g_object_set (label, "halign", GTK_ALIGN_START, NULL);
	gtk_grid_attach (grid, label, col++, row, 1, 1);

	/* FIXME when gtk_file_chooser_button allows GTK_FILE_CHOOSER_ACTION_SAVE use it */
	button = gtk_button_new_with_label (_("Please select a file…"));
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (run_cert_backup_dialog_file_chooser),
		&data);
	g_signal_connect (
		button, "focus-in-event",
		G_CALLBACK (cert_backup_dialog_sensitize),
		&data);
	gtk_grid_attach (grid, button, col++, row++, 1, 1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (button));

	/* cert chain option */
	col = 1;
	chain = gtk_check_button_new_with_mnemonic (_("_Include certificate chain in the backup"));
	gtk_grid_attach (grid, chain, col, row++, 1, 1);

	/* password */
	col = 0;
	/* To Translators: this text was copied from Firefox */
	label = gtk_label_new (_("The certificate backup password you set here protects "
		"the backup file that you are about to create.\nYou must set this password to proceed with the backup."));
	gtk_grid_attach (grid, label, col, row++, 2, 1);

	col = 0;
	label = gtk_label_new_with_mnemonic (_("_Password:"));
	g_object_set (label, "halign", GTK_ALIGN_START, NULL);
	gtk_grid_attach (grid, label, col++, row, 1, 1);

	data.entry1 = gtk_entry_new ();
	g_signal_connect(
		data.entry1, "key-release-event",
		G_CALLBACK (cert_backup_dialog_sensitize),
		&data);
	gtk_entry_set_visibility (GTK_ENTRY (data.entry1), FALSE);
	gtk_grid_attach (grid, data.entry1, col++, row++, 1, 1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (data.entry1));

	col = 0;
	label = gtk_label_new_with_mnemonic (_("_Repeat Password:"));
	g_object_set (label, "halign", GTK_ALIGN_START, NULL);
	gtk_grid_attach (grid, label, col++, row, 1, 1);

	data.entry2 = gtk_entry_new ();
	g_signal_connect(
		data.entry2, "key-release-event",
		G_CALLBACK (cert_backup_dialog_sensitize),
		&data);
	gtk_entry_set_visibility (GTK_ENTRY (data.entry2), FALSE);
	gtk_grid_attach (grid, data.entry2, col++, row++, 1, 1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (data.entry2));

	col = 0;
	label = gtk_label_new (""); /* force grid to create a row for match_label */
	gtk_grid_attach (grid, label, col++, row, 1, 1);

	data.match_label = gtk_label_new ("");
	g_object_set (data.match_label, "halign", GTK_ALIGN_START, NULL);
	markup = g_markup_printf_escaped (format, _("Passwords do not match"));
	gtk_label_set_markup (GTK_LABEL (data.match_label), markup);
	g_free (markup);
	gtk_grid_attach (grid, data.match_label, col, row++, 1, 1);
	gtk_widget_set_visible (data.match_label, FALSE);

	col = 0;
	/* To Translators: this text was copied from Firefox */
	label = gtk_label_new (_("Important:\nIf you forget your certificate backup password, "
		"you will not be able to restore this backup later.\nPlease record it in a safe location."));
	gtk_grid_attach (grid, label, col, row++, 2, 1);

	/* add widget to dialog and show all */
	gtk_widget_show_all (GTK_WIDGET (grid));
	gtk_container_add (GTK_CONTAINER (content_area), GTK_WIDGET (grid));

	data.save_button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_widget_set_sensitive (data.save_button, FALSE);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	*password = strdup (gtk_entry_get_text (GTK_ENTRY (data.entry1)));
	*save_chain = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chain));

	gtk_widget_destroy (dialog);

	return response;
}

static void
backup_cert (GtkWidget *button,
             CertPage *cp)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (cp->treeview), NULL, &iter)) {
		ECert *cert;

		gtk_tree_model_get (
			GTK_TREE_MODEL (cp->streemodel),
			&iter,
			cp->columns_count - 1,
			&cert,
			-1);

		if (cert) {
			GFile *file = NULL;
			gchar *password = NULL;
			gboolean save_chain = FALSE;

			if (run_cert_backup_dialog (cp, gtk_widget_get_toplevel (button), cert, &file, &password, &save_chain) == GTK_RESPONSE_OK) {
				if (!file) {
					e_notice (
						gtk_widget_get_toplevel (GTK_WIDGET (cp->treeview)),
						GTK_MESSAGE_ERROR, "%s", _("No file name provided"));
				} else if (cp->cert_type == E_CERT_USER) {
					GError *error = NULL;
					if (!e_cert_db_export_pkcs12_file (cert, file, password, save_chain, &error)) {
						report_and_free_error (cp, _("Failed to backup key and certificate"), error);
					}
				} else {
					g_warn_if_reached ()
					;
				}
			}

			if (file)
				g_object_unref (file);
			if (password) {
				memset (password, 0, strlen (password));
				g_free (password);
			}

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
	GtkFileChooserNative *native;
	GtkFileFilter *filter;
	GtkWidget *toplevel;
	gint i;

	toplevel = gtk_widget_get_toplevel (button);

	native = gtk_file_chooser_native_new (
		_("Select a certificate to import…"),
		GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Open"), _("_Cancel"));

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, cp->cert_filter_name);
	for (i = 0; cp->cert_mime_types[i] != NULL; i++) {
		gtk_file_filter_add_mime_type (filter, cp->cert_mime_types[i]);
	}
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (native));
		GSList *imported_certs = NULL;
		GError *error = NULL;
		gboolean import;

		/* destroy dialog to get rid of it in the GUI */
		g_object_unref (native);

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
	} else {
		g_object_unref (native);
	}
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

enum {
	MAIL_CERT_COLUMN_HOSTNAME,
	MAIL_CERT_COLUMN_SUBJECT,
	MAIL_CERT_COLUMN_ISSUER,
	MAIL_CERT_COLUMN_FINGERPRINT,
	MAIL_CERT_COLUMN_TRUST,
	MAIL_CERT_COLUMN_CAMELCERT,
	MAIL_CERT_N_COLUMNS
};

static const gchar *
cm_get_camel_cert_trust_text (CamelCertTrust trust)
{
	switch (trust) {
		case CAMEL_CERT_TRUST_UNKNOWN:
			return C_("CamelTrust", "Ask when used");
		case CAMEL_CERT_TRUST_NEVER:
			return C_("CamelTrust", "Never");
		case CAMEL_CERT_TRUST_MARGINAL:
			return C_("CamelTrust", "Marginally");
		case CAMEL_CERT_TRUST_FULLY:
			return C_("CamelTrust", "Fully");
		case CAMEL_CERT_TRUST_ULTIMATE:
			return C_("CamelTrust", "Ultimately");
		case CAMEL_CERT_TRUST_TEMPORARY:
			return C_("CamelTrust", "Temporarily");
	}

	return "???";
}

static void
cm_add_text_column (GtkTreeView *tree_view,
		    const gchar *title,
		    gint column_index,
		    gboolean expand)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	column = gtk_tree_view_column_new_with_attributes (
		title, renderer, "text", column_index, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, column_index);
	gtk_tree_view_column_set_visible (column, TRUE);
	gtk_tree_view_column_set_expand (column, expand);
	gtk_tree_view_append_column (tree_view, column);
}

static void
selection_changed_has_one_row_cb (GtkTreeSelection *selection,
				  GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_widget_set_sensitive (widget, gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static gboolean
cm_unref_camel_cert (GtkTreeModel *model,
		     GtkTreePath *path,
		     GtkTreeIter *iter,
		     gpointer user_data)
{
	CamelCert *camelcert = NULL;

	gtk_tree_model_get (model, iter, MAIL_CERT_COLUMN_CAMELCERT,  &camelcert, -1);

	if (camelcert)
		camel_cert_unref (camelcert);

	return FALSE;
}

static void
load_mail_certs (ECertManagerConfig *ecmc)
{
	GtkListStore *list_store;
	GSList *camel_certs, *link;
	CamelCertDB *certdb;

	g_return_if_fail (E_IS_CERT_MANAGER_CONFIG (ecmc));
	g_return_if_fail (ecmc->priv->mail_model != NULL);

	gtk_tree_model_foreach (ecmc->priv->mail_model, cm_unref_camel_cert, NULL);

	list_store = GTK_LIST_STORE (ecmc->priv->mail_model);
	gtk_list_store_clear (list_store);

	certdb = camel_certdb_get_default ();
	g_return_if_fail (certdb != NULL);

	camel_certs = camel_certdb_list_certs (certdb);
	for (link = camel_certs; link; link = g_slist_next (link)) {
		CamelCert *cert = link->data;
		GtkTreeIter iter;

		if (!cert)
			continue;

		camel_cert_ref (cert);
		if (!cert->rawcert)
			camel_cert_load_cert_file (cert, NULL);

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
			MAIL_CERT_COLUMN_HOSTNAME, cert->hostname,
			MAIL_CERT_COLUMN_SUBJECT, cert->subject,
			MAIL_CERT_COLUMN_ISSUER, cert->issuer,
			MAIL_CERT_COLUMN_FINGERPRINT, cert->fingerprint,
			MAIL_CERT_COLUMN_TRUST, cm_get_camel_cert_trust_text (cert->trust),
			MAIL_CERT_COLUMN_CAMELCERT, cert,
			-1);
	}

	g_slist_free_full (camel_certs, (GDestroyNotify) camel_cert_unref);
}

static GtkWidget *
cm_prepare_certificate_widget (gconstpointer data,
			       gsize length)
{
	GtkWidget *widget;

	widget = e_certificate_widget_new ();
	e_certificate_widget_set_der (E_CERTIFICATE_WIDGET (widget), data, length);

	return widget;
}

static void
mail_cert_view_cb (GtkWidget *button,
		   GtkTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelCert *camel_cert = NULL;
	ECert *cert;

	g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, MAIL_CERT_COLUMN_CAMELCERT, &camel_cert, -1);

	if (!camel_cert)
		return;

	g_return_if_fail (camel_cert->rawcert != NULL);

	cert = e_cert_new_from_der ((gchar *) g_bytes_get_data (camel_cert->rawcert, NULL), g_bytes_get_size (camel_cert->rawcert));
	if (cert) {
		open_cert_viewer (button, cert);
		g_object_unref (cert);
	}
}

static gboolean
mail_cert_edit_trust (GtkWidget *parent,
		      CamelCert *camel_cert)
{
	GtkWidget *dialog, *label, *expander, *content_area, *certificate_widget;
	GtkWidget *runknown, *rtemporary, *rnever, *rmarginal, *rfully, *rultimate;
	GtkGrid *grid;
	gchar *text;
	gboolean changed = FALSE;
	gint row;

	g_return_val_if_fail (camel_cert != NULL, FALSE);
	g_return_val_if_fail (camel_cert->rawcert != NULL, FALSE);

	certificate_widget = cm_prepare_certificate_widget (g_bytes_get_data (camel_cert->rawcert, NULL), g_bytes_get_size (camel_cert->rawcert));

	g_return_val_if_fail (certificate_widget != NULL, FALSE);

	dialog = gtk_dialog_new_with_buttons (
		_("Change certificate trust"), parent ? GTK_WINDOW (parent) : NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
		_("_Cancel"), GTK_RESPONSE_CLOSE,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	grid = GTK_GRID (gtk_grid_new ());

	text = g_strdup_printf (_("Change trust for the host “%s”:"), camel_cert->hostname);
	label = gtk_label_new (text);
	g_object_set (G_OBJECT (label),
		"margin-bottom", 4,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"vexpand", FALSE,
		NULL);
	gtk_grid_attach (grid, label, 0, 0, 1, 1);
	g_free (text);

	#define add_radio(_radio, _title, _trust) G_STMT_START { \
		_radio = gtk_radio_button_new_with_mnemonic (runknown ? gtk_radio_button_get_group (GTK_RADIO_BUTTON (runknown)) : NULL, _title); \
		gtk_widget_set_margin_start (_radio, 12); \
		if (camel_cert->trust == (_trust)) \
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (_radio), TRUE); \
		gtk_grid_attach (grid, _radio, 0, row, 1, 1); \
		row++; \
		} G_STMT_END

	runknown = NULL;
	row = 1;

	add_radio (runknown, C_("CamelTrust", "_Ask when used"), CAMEL_CERT_TRUST_UNKNOWN);
	add_radio (rnever, C_("CamelTrust", "_Never trust this certificate"), CAMEL_CERT_TRUST_NEVER);
	add_radio (rtemporary, C_("CamelTrust", "_Temporarily trusted (this session only)"), CAMEL_CERT_TRUST_TEMPORARY);
	add_radio (rmarginal, C_("CamelTrust", "_Marginally trusted"), CAMEL_CERT_TRUST_MARGINAL);
	add_radio (rfully, C_("CamelTrust", "_Fully trusted"), CAMEL_CERT_TRUST_FULLY);
	add_radio (rultimate, C_("CamelTrust", "_Ultimately trusted"), CAMEL_CERT_TRUST_ULTIMATE);

	#undef add_radio

	label = gtk_label_new (_("Before trusting this site, you should examine its certificate and its policy and procedures (if available)."));
	g_object_set (G_OBJECT (label),
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"vexpand", FALSE,
		"xalign", 0.0,
		"yalign", 0.0,
		"max-width-chars", 60,
		"width-chars", 40,
		"wrap", TRUE,
		NULL);
	gtk_grid_attach (grid, label, 0, row, 1, 1);
	row++;

	expander = gtk_expander_new_with_mnemonic (_("_Display certificate"));
	g_object_set (G_OBJECT (label),
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_START,
		"hexpand", TRUE,
		"vexpand", FALSE,
		"margin", 6,
		NULL);
	gtk_container_add (GTK_CONTAINER (expander), certificate_widget);
	gtk_grid_attach (grid, expander, 0, row, 1, 1);
	row++;

	gtk_container_add (GTK_CONTAINER (content_area), GTK_WIDGET (grid));
	gtk_widget_show_all (content_area);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		CamelCertTrust trust = CAMEL_CERT_TRUST_UNKNOWN;

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rnever)))
			trust = CAMEL_CERT_TRUST_NEVER;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rmarginal)))
			trust = CAMEL_CERT_TRUST_MARGINAL;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rfully)))
			trust = CAMEL_CERT_TRUST_FULLY;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rultimate)))
			trust = CAMEL_CERT_TRUST_ULTIMATE;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rtemporary)))
			trust = CAMEL_CERT_TRUST_TEMPORARY;

		changed = trust != camel_cert->trust;
		if (changed)
			camel_cert->trust = trust;
	}

	gtk_widget_destroy (dialog);

	return changed;
}

static void
mail_cert_edit_trust_cb (GtkWidget *button,
			 GtkTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelCert *camel_cert = NULL;
	CamelCertDB *certdb;
	GtkWidget *parent_window;

	g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, MAIL_CERT_COLUMN_CAMELCERT, &camel_cert, -1);

	if (!camel_cert)
		return;

	g_return_if_fail (camel_cert != NULL);

	certdb = camel_certdb_get_default ();
	g_return_if_fail (certdb != NULL);

	parent_window = gtk_widget_get_toplevel (button);
	if (!parent_window || !GTK_IS_WINDOW (parent_window))
		parent_window = NULL;

	if (mail_cert_edit_trust (parent_window, camel_cert)) {
		camel_certdb_touch (certdb);
		camel_certdb_save (certdb);

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			MAIL_CERT_COLUMN_TRUST, cm_get_camel_cert_trust_text (camel_cert->trust),
			-1);
	}
}

static void
mail_cert_delete_cb (GtkButton *button,
		     GtkTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, next;
	gboolean next_valid = TRUE;
	CamelCert *camel_cert = NULL;
	CamelCertDB *certdb;

	g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, MAIL_CERT_COLUMN_CAMELCERT, &camel_cert, -1);

	if (!camel_cert)
		return;

	g_return_if_fail (camel_cert->rawcert != NULL);

	certdb = camel_certdb_get_default ();
	g_return_if_fail (certdb != NULL);

	camel_certdb_remove_host (certdb, camel_cert->hostname, camel_cert->fingerprint);
	camel_certdb_touch (certdb);
	camel_certdb_save (certdb);

	next = iter;
	if (!gtk_tree_model_iter_next (model, &next)) {
		next = iter;
		next_valid = gtk_tree_model_iter_previous (model, &next);
	}

	if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
		camel_cert_unref (camel_cert);

	if (next_valid)
		gtk_tree_selection_select_iter (selection, &next);
}

static void
mail_cert_update_cb (GtkButton *button,
		     ECertManagerConfig *ecmc)
{
	gboolean had_selected;
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *hostname = NULL, *fingerprint = NULL;

	g_return_if_fail (E_IS_CERT_MANAGER_CONFIG (ecmc));
	g_return_if_fail (ecmc->priv->mail_tree_view);

	selection = gtk_tree_view_get_selection (ecmc->priv->mail_tree_view);
	had_selected = gtk_tree_selection_get_selected (selection, &model, &iter);

	if (had_selected) {
		gtk_tree_model_get (model, &iter,
			MAIL_CERT_COLUMN_HOSTNAME, &hostname,
			MAIL_CERT_COLUMN_FINGERPRINT, &fingerprint,
			-1);
	}

	load_mail_certs (ecmc);

	if (had_selected && hostname && fingerprint &&
	    gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *sec_hostname = NULL, *sec_fingerprint = NULL;

			gtk_tree_model_get (model, &iter,
				MAIL_CERT_COLUMN_HOSTNAME, &sec_hostname,
				MAIL_CERT_COLUMN_FINGERPRINT, &sec_fingerprint,
				-1);

			if (g_strcmp0 (hostname, sec_hostname) == 0 &&
			    g_strcmp0 (fingerprint, sec_fingerprint) == 0) {
				gtk_tree_selection_select_iter (selection, &iter);
				g_free (sec_hostname);
				g_free (sec_fingerprint);
				break;
			}

			g_free (sec_hostname);
			g_free (sec_fingerprint);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	g_free (hostname);
	g_free (fingerprint);
}

static void
cm_add_mail_certificate_page (ECertManagerConfig *ecmc,
			      GtkNotebook *notebook)
{
	GtkGrid *grid;
	GtkWidget *label, *tree_view, *scrolled_window, *button_box, *button;
	GtkTreeSelection *selection;

	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
	g_return_if_fail (E_IS_CERT_MANAGER_CONFIG (ecmc));
	g_return_if_fail (ecmc->priv->mail_model == NULL);

	ecmc->priv->mail_model = GTK_TREE_MODEL (gtk_list_store_new (MAIL_CERT_N_COLUMNS,
		G_TYPE_STRING,    /* hostname */
		G_TYPE_STRING,    /* subject */
		G_TYPE_STRING,    /* issuer */
		G_TYPE_STRING,    /* fingerprint */
		G_TYPE_STRING,    /* trust */
		G_TYPE_POINTER)); /* CamelCert */

	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (G_OBJECT (grid),
		"hexpand", TRUE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"margin", 2,
		NULL);

	label = gtk_label_new (_("You have certificates on file that identify these mail servers:"));
	g_object_set (G_OBJECT (label),
		"hexpand", TRUE,
		"vexpand", FALSE,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_START,
		"margin", 4,
		NULL);
	gtk_grid_attach (grid, label, 0, 0, 2, 1);

	tree_view = gtk_tree_view_new_with_model (ecmc->priv->mail_model);
	g_object_set (G_OBJECT (tree_view),
		"hexpand", TRUE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"name", "mail-certs",
		NULL);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (scrolled_window),
		"hexpand", TRUE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
	gtk_grid_attach (grid, scrolled_window, 0, 1, 1, 1);

	cm_add_text_column (GTK_TREE_VIEW (tree_view), _("Host name"), MAIL_CERT_COLUMN_HOSTNAME, TRUE);
	cm_add_text_column (GTK_TREE_VIEW (tree_view), _("Subject"), MAIL_CERT_COLUMN_SUBJECT, FALSE);
	cm_add_text_column (GTK_TREE_VIEW (tree_view), _("Issuer"), MAIL_CERT_COLUMN_ISSUER, FALSE);
	cm_add_text_column (GTK_TREE_VIEW (tree_view), _("Fingerprint"), MAIL_CERT_COLUMN_FINGERPRINT, FALSE);
	cm_add_text_column (GTK_TREE_VIEW (tree_view), _("Trust"), MAIL_CERT_COLUMN_TRUST, FALSE);

	button_box = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	g_object_set (G_OBJECT (button_box),
		"hexpand", FALSE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_START,
		"margin", 2,
		"spacing", 6,
		NULL);
	gtk_grid_attach (grid, button_box, 1, 1, 1, 1);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	button = gtk_button_new_with_mnemonic (_("_View"));
	gtk_container_add (GTK_CONTAINER (button_box), button);
	g_signal_connect_object (selection, "changed", G_CALLBACK (selection_changed_has_one_row_cb), button, 0);
	g_signal_connect_object (button, "clicked", G_CALLBACK (mail_cert_view_cb), tree_view, 0);

	button = gtk_button_new_with_mnemonic (_("_Edit Trust"));
	gtk_container_add (GTK_CONTAINER (button_box), button);
	g_signal_connect_object (selection, "changed", G_CALLBACK (selection_changed_has_one_row_cb), button, 0);
	g_signal_connect_object (button, "clicked", G_CALLBACK (mail_cert_edit_trust_cb), tree_view, 0);

	button = gtk_button_new_with_mnemonic (_("_Delete"));
	gtk_container_add (GTK_CONTAINER (button_box), button);
	g_signal_connect_object (selection, "changed", G_CALLBACK (selection_changed_has_one_row_cb), button, 0);
	g_signal_connect_object (button, "clicked", G_CALLBACK (mail_cert_delete_cb), tree_view, 0);

	button = gtk_button_new_with_mnemonic (_("_Update"));
	gtk_container_add (GTK_CONTAINER (button_box), button);
	g_signal_connect_object (button, "clicked", G_CALLBACK (mail_cert_update_cb), ecmc, 0);

	gtk_widget_show_all (GTK_WIDGET (grid));
	gtk_notebook_append_page (notebook, GTK_WIDGET (grid), gtk_label_new (_("Mail")));

	ecmc->priv->mail_tree_view = GTK_TREE_VIEW (tree_view);

	/* to have sensitivity updated */
	g_signal_emit_by_name (selection, "changed", 0);
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

typedef struct _LoadAllCertsAsyncData
{
	ECertManagerConfig *ecmc;
	GCancellable *cancellable;
	GSList *ecerts;
	gint tries;
} LoadAllCertsAsyncData;

static void
load_all_certs_async_data_free (gpointer ptr)
{
	LoadAllCertsAsyncData *data = ptr;

	if (data) {
		g_clear_object (&data->ecmc);
		g_clear_object (&data->cancellable);
		g_slist_free_full (data->ecerts, g_object_unref);
		g_slice_free (LoadAllCertsAsyncData, data);
	}
}

static gboolean
load_all_certs_done_idle_cb (gpointer user_data)
{
	LoadAllCertsAsyncData *data = user_data;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (E_IS_CERT_MANAGER_CONFIG (data->ecmc), FALSE);

	if (!g_cancellable_is_cancelled (data->cancellable)) {
		ECertManagerConfig *ecmc = data->ecmc;
		GSList *link;

		unload_certs (data->ecmc->priv->yourcerts_page);
		unload_certs (data->ecmc->priv->contactcerts_page);
		unload_certs (data->ecmc->priv->authoritycerts_page);

		for (link = data->ecerts; link; link = g_slist_next (link)) {
			ECert *cert = link->data;
			ECertType ct;

			if (!cert)
				continue;

			ct = e_cert_get_cert_type (cert);
			if (ct == data->ecmc->priv->yourcerts_page->cert_type) {
				add_cert (data->ecmc->priv->yourcerts_page, g_object_ref (cert));
			} else if (ct == data->ecmc->priv->authoritycerts_page->cert_type) {
				add_cert (data->ecmc->priv->authoritycerts_page, g_object_ref (cert));
			} else if (ct == data->ecmc->priv->contactcerts_page->cert_type || (ct != E_CERT_CA && ct != E_CERT_USER)) {
				add_cert (data->ecmc->priv->contactcerts_page, g_object_ref (cert));
			}
		}

		/* expand all three trees */
		gtk_tree_view_expand_all (ECMC_TREE_VIEW (yourcerts_page));
		gtk_tree_view_expand_all (ECMC_TREE_VIEW (contactcerts_page));
		gtk_tree_view_expand_all (ECMC_TREE_VIEW (authoritycerts_page));

		/* Now load settings of each treeview */
		load_treeview_state (ECMC_TREE_VIEW (yourcerts_page));
		load_treeview_state (ECMC_TREE_VIEW (contactcerts_page));
		load_treeview_state (ECMC_TREE_VIEW (authoritycerts_page));
	}

	return FALSE;
}

static gpointer
load_all_certs_thread (gpointer user_data)
{
	LoadAllCertsAsyncData *data = user_data;
	CERTCertList *certList;
	CERTCertListNode *node;

	g_return_val_if_fail (data != NULL, NULL);

	certList = PK11_ListCerts (PK11CertListUnique, NULL);

	for (node = CERT_LIST_HEAD (certList);
	     !CERT_LIST_END (node, certList) && !g_cancellable_is_cancelled (data->cancellable);
	     node = CERT_LIST_NEXT (node)) {
		ECert *cert = e_cert_new (CERT_DupCertificate ((CERTCertificate *) node->cert));

		data->ecerts = g_slist_prepend (data->ecerts, cert);
	}

	CERT_DestroyCertList (certList);

	g_idle_add_full (G_PRIORITY_HIGH_IDLE, load_all_certs_done_idle_cb, data, load_all_certs_async_data_free);

	return NULL;
}

static gboolean
load_all_threads_try_create_thread (gpointer user_data)
{
	LoadAllCertsAsyncData *data = user_data;
	GThread *thread;
	GError *error = NULL;

	g_return_val_if_fail (data != NULL, FALSE);

	if (data->tries > 10 || g_cancellable_is_cancelled (data->cancellable)) {
		load_all_certs_async_data_free (data);
		return FALSE;
	}

	thread = g_thread_try_new (NULL, load_all_certs_thread, data, &error);
	if (g_error_matches (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN)) {
		data->tries++;

		g_timeout_add (250, load_all_threads_try_create_thread, data);
	} else if (!thread) {
		g_warning ("%s: Failed to create thread: %s", G_STRFUNC, error ? error->message : "Unknown error");
	} else {
		g_thread_unref (thread);
	}

	g_clear_error (&error);

	return FALSE;
}

static void
load_all_certs (ECertManagerConfig *ecmc)
{
	LoadAllCertsAsyncData *data;

	g_return_if_fail (E_IS_CERT_MANAGER_CONFIG (ecmc));

	if (ecmc->priv->load_all_certs_cancellable) {
		g_cancellable_cancel (ecmc->priv->load_all_certs_cancellable);
		g_clear_object (&ecmc->priv->load_all_certs_cancellable);
	}

	ecmc->priv->load_all_certs_cancellable = g_cancellable_new ();

	data = g_slice_new0 (LoadAllCertsAsyncData);
	data->ecmc = g_object_ref (ecmc);
	data->cancellable = g_object_ref (ecmc->priv->load_all_certs_cancellable);
	data->ecerts = NULL;
	data->tries = 0;

	load_all_threads_try_create_thread (data);
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

	load_all_certs (ecmc);
	load_mail_certs (ecmc);

	load_treeview_state (ecmc->priv->mail_tree_view);

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

	if (cp->backup_button)
		g_signal_connect(
			cp->backup_button, "clicked",
			G_CALLBACK (backup_cert),
			cp);
}

static void
cert_manager_config_window_hide (ECertManagerConfig *ecmc,
                                 EPreferencesWindow *epw)
{
	g_return_if_fail (ecmc);

	save_treeview_state (ECMC_TREE_VIEW (yourcerts_page));
	save_treeview_state (ECMC_TREE_VIEW (contactcerts_page));
	save_treeview_state (ECMC_TREE_VIEW (authoritycerts_page));
	save_treeview_state (ecmc->priv->mail_tree_view);
}

static void
free_cert (GtkTreeModel *model,
           GtkTreePath *path,
           GtkTreeIter *iter,
           gpointer user_data)
{
	CertPage *cp = user_data;
	ECert *cert = NULL;

	gtk_tree_model_get (model, iter, cp->columns_count - 1, &cert, -1);

	if (cert) {
		/* Double unref: one for gtk_tree_model_get() and one for e_cert_new() */
		g_object_unref (cert);
		g_object_unref (cert);
	}
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

	g_clear_pointer (&cp->root_hash, g_hash_table_unref);
	g_free (cp);
}

static void
cert_manager_config_dispose (GObject *object)
{
	ECertManagerConfig *ecmc = E_CERT_MANAGER_CONFIG (object);

	g_clear_pointer (&ecmc->priv->yourcerts_page, cert_page_free);
	g_clear_pointer (&ecmc->priv->contactcerts_page, cert_page_free);
	g_clear_pointer (&ecmc->priv->authoritycerts_page, cert_page_free);

	if (ecmc->priv->mail_model) {
		gtk_tree_model_foreach (ecmc->priv->mail_model, cm_unref_camel_cert, NULL);
		g_clear_object (&ecmc->priv->mail_model);
	}

	g_clear_object (&ecmc->priv->builder);

	if (ecmc->priv->pref_window) {
		g_signal_handlers_disconnect_matched (ecmc->priv->pref_window, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ecmc);
		ecmc->priv->pref_window = NULL;
	}

	if (ecmc->priv->load_all_certs_cancellable) {
		g_cancellable_cancel (ecmc->priv->load_all_certs_cancellable);
		g_clear_object (&ecmc->priv->load_all_certs_cancellable);
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
	GtkWidget *parent, *widget;
	CertPage *cp;

	ecmc->priv = e_cert_manager_config_get_instance_private (ecmc);

	/* We need to peek the db here to make sure it (and NSS) are fully initialized. */
	e_cert_db_peek ();

	ecmc->priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (ecmc->priv->builder, "smime-ui.ui");

	cp = g_new0 (CertPage, 1);
	ecmc->priv->yourcerts_page = cp;
	cp->treeview = GTK_TREE_VIEW (e_builder_get_widget (ecmc->priv->builder, "yourcerts-treeview"));
	cp->streemodel = NULL;
	cp->view_button = e_builder_get_widget (ecmc->priv->builder, "your-view-button");
	cp->backup_button = e_builder_get_widget (ecmc->priv->builder, "your-backup-button");
	cp->backup_all_button = e_builder_get_widget (ecmc->priv->builder, "your-backup-all-button");
	cp->edit_button = NULL;
	cp->import_button = e_builder_get_widget (ecmc->priv->builder, "your-import-button");
	cp->delete_button = e_builder_get_widget (ecmc->priv->builder, "your-delete-button");
	cp->columns = yourcerts_columns;
	cp->columns_count = G_N_ELEMENTS (yourcerts_columns);
	cp->cert_type = E_CERT_USER;
	cp->cert_filter_name = _("All PKCS12 files");
	cp->cert_mime_types = yourcerts_mime_types;
	initialize_ui (cp);

	cp = g_new0 (CertPage, 1);
	ecmc->priv->contactcerts_page = cp;
	cp->treeview = GTK_TREE_VIEW (e_builder_get_widget (ecmc->priv->builder, "contactcerts-treeview"));
	cp->streemodel = NULL;
	cp->view_button = e_builder_get_widget (ecmc->priv->builder, "contact-view-button");
	cp->backup_button = NULL;
	cp->backup_all_button = NULL;
	cp->edit_button = e_builder_get_widget (ecmc->priv->builder, "contact-edit-button");
	cp->import_button = e_builder_get_widget (ecmc->priv->builder, "contact-import-button");
	cp->delete_button = e_builder_get_widget (ecmc->priv->builder, "contact-delete-button");
	cp->columns = contactcerts_columns;
	cp->columns_count = G_N_ELEMENTS (contactcerts_columns);
	cp->cert_type = E_CERT_CONTACT;
	cp->cert_filter_name = _("All email certificate files");
	cp->cert_mime_types = contactcerts_mime_types;
	initialize_ui (cp);

	cp = g_new0 (CertPage, 1);
	ecmc->priv->authoritycerts_page = cp;
	cp->treeview = GTK_TREE_VIEW (e_builder_get_widget (ecmc->priv->builder, "authoritycerts-treeview"));
	cp->streemodel = NULL;
	cp->view_button = e_builder_get_widget (ecmc->priv->builder, "authority-view-button");
	cp->backup_button = NULL;
	cp->backup_all_button = NULL;
	cp->edit_button = e_builder_get_widget (ecmc->priv->builder, "authority-edit-button");
	cp->import_button = e_builder_get_widget (ecmc->priv->builder, "authority-import-button");
	cp->delete_button = e_builder_get_widget (ecmc->priv->builder, "authority-delete-button");
	cp->columns = authoritycerts_columns;
	cp->columns_count = G_N_ELEMENTS (authoritycerts_columns);
	cp->cert_type = E_CERT_CA;
	cp->cert_filter_name = _("All CA certificate files");
	cp->cert_mime_types = authoritycerts_mime_types;
	initialize_ui (cp);

	cm_add_mail_certificate_page (ecmc, GTK_NOTEBOOK (e_builder_get_widget (ecmc->priv->builder, "cert-manager-notebook")));

	/* Run this in an idle callback so Evolution has a chance to
	 * fully initialize itself and start its main loop before we
	 * load certificates, since doing so may trigger a password
	 * dialog, and dialogs require a main loop.
	 * Schedule with priority higher than gtk+ uses for animations
	 * (check docs for G_PRIORITY_HIGH_IDLE). */
	g_idle_add_full (G_PRIORITY_DEFAULT, (GSourceFunc) populate_ui, ecmc, NULL);

	/* Disconnect cert-manager-notebook from it's window and attach it
	 * to this ECertManagerConfig */
	widget = e_builder_get_widget (ecmc->priv->builder, "cert-manager-notebook");
	parent = gtk_widget_get_parent (widget);
	gtk_container_remove (GTK_CONTAINER (parent), widget);
	gtk_box_pack_start (GTK_BOX (ecmc), widget, TRUE, TRUE, 0);
	gtk_widget_show_all (widget);

	/* FIXME: remove when implemented */
	gtk_widget_set_visible (ecmc->priv->yourcerts_page->backup_all_button, FALSE);
}

GtkWidget *
e_cert_manager_config_new (EPreferencesWindow *window)
{
	ECertManagerConfig *ecmc;

	ecmc = g_object_new (E_TYPE_CERT_MANAGER_CONFIG, "preferences-window", window, NULL);

	return GTK_WIDGET (ecmc);
}

GtkWidget *
e_cert_manager_new_certificate_viewer (GtkWindow *parent,
                                       ECert *cert)
{
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *widget, *certificate_widget;
	gchar *data = NULL;
	guint32 len = 0;
	const gchar *title;

	g_return_val_if_fail (cert != NULL, NULL);

	if (!e_cert_get_raw_der (cert, &data, &len)) {
		data = NULL;
		len = 0;
	}

	certificate_widget = cm_prepare_certificate_widget (data, (gsize) len);

	title = e_cert_get_cn (cert);
	if (!title || !*title)
		title = e_cert_get_email (cert);
	if (!title || !*title)
		title = e_cert_get_subject_name (cert);

	dialog = gtk_dialog_new_with_buttons (
		title, parent,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Close"), GTK_RESPONSE_CLOSE,
		NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = GTK_WIDGET (certificate_widget);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);
	gtk_widget_show_all (widget);

	return dialog;
}
