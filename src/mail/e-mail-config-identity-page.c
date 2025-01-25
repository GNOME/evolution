/*
 * e-mail-config-identity-page.c
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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include "e-util/e-util.h"
#include "e-mail-config-identity-page.h"

struct _EMailConfigIdentityPagePrivate {
	ESource *identity_source;
	ESourceRegistry *registry;
	gboolean show_account_info;
	gboolean show_email_address;
	gboolean show_instructions;
	gboolean show_signatures;
	gboolean show_autodiscover_check;
	GtkWidget *autodiscover_check;	/* not referenced */
	GtkWidget *display_name_entry;	/* not referenced */
	GtkWidget *name_entry;		/* not referenced */
	GtkWidget *address_entry;	/* not referenced */
	GtkWidget *reply_to_entry;	/* not referenced */
	GtkWidget *aliases_treeview;	/* not referenced */
	GtkWidget *aliases_add_button;	/* not referenced */
	GtkWidget *aliases_edit_button;	/* not referenced */
	GtkWidget *aliases_remove_button; /* not referenced */
};

enum {
	PROP_0,
	PROP_IDENTITY_SOURCE,
	PROP_REGISTRY,
	PROP_SHOW_ACCOUNT_INFO,
	PROP_SHOW_EMAIL_ADDRESS,
	PROP_SHOW_INSTRUCTIONS,
	PROP_SHOW_SIGNATURES,
	PROP_SHOW_AUTODISCOVER_CHECK
};

/* Forward Declarations */
static void	e_mail_config_identity_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailConfigIdentityPage, e_mail_config_identity_page, GTK_TYPE_SCROLLED_WINDOW,
	G_ADD_PRIVATE (EMailConfigIdentityPage)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_identity_page_interface_init))

static gboolean
mail_config_identity_page_is_email (const gchar *email_address)
{
	const gchar *cp;

	/* Make sure we have a '@' between a name and domain part. */
	cp = strchr (email_address, '@');

	return (cp != NULL && cp != email_address && *(cp + 1) != '\0');
}

static void
mail_config_identity_page_signature_editor_created_cb (GObject *source_object,
						       GAsyncResult *result,
						       gpointer user_data)
{
	GtkWidget *editor;
	GError *error = NULL;

	g_return_if_fail (result != NULL);

	editor = e_mail_signature_editor_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create signature editor: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		gtk_window_set_position (GTK_WINDOW (editor), GTK_WIN_POS_CENTER);
		gtk_widget_show (editor);
	}
}

static void
mail_config_identity_page_add_signature_cb (GtkButton *button,
                                            EMailConfigIdentityPage *page)
{
	ESourceRegistry *registry;

	registry = e_mail_config_identity_page_get_registry (page);

	e_mail_signature_editor_new (registry, NULL,
		mail_config_identity_page_signature_editor_created_cb, NULL);
}

static void
mail_config_identity_page_add_alias_clicked_cb (GtkWidget *button,
						gpointer user_data)
{
	EMailConfigIdentityPage *page = user_data;
	GtkTreeModel *model;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	tree_view = GTK_TREE_VIEW (page->priv->aliases_treeview);
	model = gtk_tree_view_get_model (tree_view);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	path = gtk_tree_model_get_path (model, &iter);
	column = gtk_tree_view_get_column (tree_view, 0);
	gtk_tree_view_set_cursor (tree_view, path, column, TRUE);
	gtk_tree_view_row_activated (tree_view, path, column);
	gtk_tree_path_free (path);
}

static void
mail_config_identity_page_edit_alias_clicked_cb (GtkWidget *button,
						 gpointer user_data)
{
	EMailConfigIdentityPage *page = user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeViewColumn *focus_col;
	GtkTreeView *treeview;

	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	treeview = GTK_TREE_VIEW (page->priv->aliases_treeview);
	selection = gtk_tree_view_get_selection (treeview);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	focus_col = gtk_tree_view_get_column (treeview, 0);
	path = gtk_tree_model_get_path (model, &iter);

	if (path) {
		gtk_tree_view_set_cursor (treeview, path, focus_col, TRUE);
		gtk_tree_path_free (path);
	}
}

static void
mail_config_identity_page_remove_alias_clicked_cb (GtkWidget *button,
						   gpointer user_data)
{
	EMailConfigIdentityPage *page = user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid = FALSE;
	gint len;

	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page->priv->aliases_treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	/* Get the path and move to the previous node */
	path = gtk_tree_model_get_path (model, &iter);
	if (path)
		valid = gtk_tree_path_prev (path);

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	len = gtk_tree_model_iter_n_children (model, NULL);
	if (len > 0) {
		if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (model), &iter)) {
			gtk_tree_selection_select_iter (selection, &iter);
		} else {
			if (path && valid) {
				gtk_tree_model_get_iter (model, &iter, path);
				gtk_tree_selection_select_iter (selection, &iter);
			}
		}
	} else {
		gtk_widget_set_sensitive (page->priv->aliases_edit_button, FALSE);
		gtk_widget_set_sensitive (page->priv->aliases_remove_button, FALSE);
	}

	gtk_widget_grab_focus (page->priv->aliases_treeview);
	gtk_tree_path_free (path);

	e_mail_config_page_changed (E_MAIL_CONFIG_PAGE (page));
}

static void
mail_config_identity_page_aliases_cell_edited_cb (GtkCellRendererText *cell,
						  gchar *path_string,
						  gchar *new_text,
						  gpointer user_data)
{
	EMailConfigIdentityPage *page = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (page->priv->aliases_treeview));
	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	if (!new_text || !(*g_strstrip (new_text))) {
		mail_config_identity_page_remove_alias_clicked_cb (NULL, page);
	} else {
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, new_text, -1);
		e_mail_config_page_changed (E_MAIL_CONFIG_PAGE (page));
	}
}

static void
mail_config_identity_page_aliases_cell_editing_canceled_cb (GtkCellRenderer *cell,
							    gpointer user_data)
{
	EMailConfigIdentityPage *page = user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *text = NULL;

	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page->priv->aliases_treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, 0, &text, -1);

	if (!text || !*text)
		mail_config_identity_page_remove_alias_clicked_cb (NULL, page);

	g_free (text);
}

static void
mail_config_identity_page_aliases_selection_changed_cb (GtkTreeSelection *selection,
							gpointer user_data)
{
	EMailConfigIdentityPage *page = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_widget_set_sensitive (page->priv->aliases_edit_button, TRUE);
		gtk_widget_set_sensitive (page->priv->aliases_remove_button, TRUE);
	} else {
		gtk_widget_set_sensitive (page->priv->aliases_edit_button, FALSE);
		gtk_widget_set_sensitive (page->priv->aliases_remove_button, FALSE);
	}
}

static void
mail_config_identity_page_fill_aliases (EMailConfigIdentityPage *page,
					ESourceMailIdentity *extension)
{
	GtkListStore *list_store;
	GtkTreeIter iter;
	CamelInternetAddress *inetaddress;
	gchar *aliases;
	gint ii, len;

	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));
	g_return_if_fail (E_IS_SOURCE_MAIL_IDENTITY (extension));

	aliases = e_source_mail_identity_dup_aliases (extension);
	if (!aliases)
		return;

	inetaddress = camel_internet_address_new ();
	len = camel_address_decode (CAMEL_ADDRESS (inetaddress), aliases);
	g_free (aliases);

	if (len <= 0) {
		g_clear_object (&inetaddress);
		return;
	}

	list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (page->priv->aliases_treeview)));

	for (ii = 0; ii < len; ii++) {
		const gchar *name = NULL, *email = NULL;

		if (camel_internet_address_get (inetaddress, ii, &name, &email)) {
			gchar *formatted;

			formatted = camel_internet_address_format_address (name, email);
			if (formatted && *formatted) {
				gtk_list_store_append (list_store, &iter);
				gtk_list_store_set (list_store, &iter, 0, formatted, -1);
			}

			g_free (formatted);
		}
	}

	g_clear_object (&inetaddress);
}

static void
mail_config_identity_page_set_registry (EMailConfigIdentityPage *page,
                                        ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (page->priv->registry == NULL);

	page->priv->registry = g_object_ref (registry);
}

static void
mail_config_identity_page_set_identity_source (EMailConfigIdentityPage *page,
                                               ESource *identity_source)
{
	g_return_if_fail (E_IS_SOURCE (identity_source));
	g_return_if_fail (page->priv->identity_source == NULL);

	page->priv->identity_source = g_object_ref (identity_source);
}

static void
mail_config_identity_page_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_SOURCE:
			mail_config_identity_page_set_identity_source (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_REGISTRY:
			mail_config_identity_page_set_registry (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_ACCOUNT_INFO:
			e_mail_config_identity_page_set_show_account_info (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_EMAIL_ADDRESS:
			e_mail_config_identity_page_set_show_email_address (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_INSTRUCTIONS:
			e_mail_config_identity_page_set_show_instructions (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_SIGNATURES:
			e_mail_config_identity_page_set_show_signatures (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_AUTODISCOVER_CHECK:
			e_mail_config_identity_page_set_show_autodiscover_check (
				E_MAIL_CONFIG_IDENTITY_PAGE (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_identity_page_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_identity_page_get_identity_source (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_config_identity_page_get_registry (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_ACCOUNT_INFO:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_account_info (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_EMAIL_ADDRESS:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_email_address (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_INSTRUCTIONS:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_instructions (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_SIGNATURES:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_signatures (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;

		case PROP_SHOW_AUTODISCOVER_CHECK:
			g_value_set_boolean (
				value,
				e_mail_config_identity_page_get_show_autodiscover_check (
				E_MAIL_CONFIG_IDENTITY_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_identity_page_dispose (GObject *object)
{
	EMailConfigIdentityPage *self = E_MAIL_CONFIG_IDENTITY_PAGE (object);

	g_clear_object (&self->priv->identity_source);
	g_clear_object (&self->priv->registry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_identity_page_parent_class)->dispose (object);
}

static void
mail_config_identity_page_constructed (GObject *object)
{
	EMailConfigIdentityPage *page;
	ESource *source;
	ESourceRegistry *registry;
	ESourceMailIdentity *extension;
	GtkLabel *label;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkWidget *main_box, *widget;
	GtkWidget *container;
	GtkWidget *scrolledwindow;
	GtkSizeGroup *size_group;
	const gchar *extension_name;
	const gchar *text;
	gchar *markup;

	page = E_MAIL_CONFIG_IDENTITY_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_identity_page_parent_class)->constructed (object);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	registry = e_mail_config_identity_page_get_registry (page);
	source = e_mail_config_identity_page_get_identity_source (page);
	extension = e_source_get_extension (source, extension_name);

	main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_valign (main_box, GTK_ALIGN_FILL);
	gtk_widget_set_vexpand (main_box, TRUE);

	/* This keeps all mnemonic labels the same width. */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	text = _("Please enter your name and email address below. "
		 "The “optional” fields below do not need to be filled "
		 "in, unless you wish to include this information in email "
		 "you send.");
	widget = gtk_label_new (text);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);

	e_binding_bind_property (
		page, "show-instructions",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	/*** Account Information ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);

	e_binding_bind_property (
		page, "show-account-info",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	container = widget;

	text = _("Account Information");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("_Name:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	page->priv->display_name_entry = widget;

	e_binding_bind_object_text_property (
		source, "display-name",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* This entry affects the "check-complete" result. */
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	text = _("The above name will be used to identify this account.\n"
		 "Use for example, “Work” or “Personal”.");
	widget = gtk_label_new (text);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	/*** Required Information ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Required Information");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Full Nam_e:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	page->priv->name_entry = widget;

	e_binding_bind_object_text_property (
		extension, "name",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* This entry affects the "check-complete" result. */
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	text = _("Email _Address:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		page, "show-email-address",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	page->priv->address_entry = widget;

	e_binding_bind_object_text_property (
		extension, "address",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		page, "show-email-address",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	/* This entry affects the "check-complete" result. */
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	/*** Optional Information ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Optional Information");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 3, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Re_ply-To:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 2, 1);
	gtk_widget_show (widget);

	page->priv->reply_to_entry = widget;

	e_binding_bind_object_text_property (
		extension, "reply-to",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* This entry affects the "check-complete" result. */
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	text = _("Or_ganization:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 2, 1);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		extension, "organization",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	text = _("Si_gnature:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		page, "show-signatures",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	label = GTK_LABEL (widget);

	widget = e_mail_signature_combo_box_new (registry);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 3, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		extension, "signature-uid",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		page, "show-signatures",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	text = _("Add Ne_w Signature…");
	widget = gtk_button_new_with_mnemonic (text);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 3, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		page, "show-signatures",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (mail_config_identity_page_add_signature_cb), page);

	widget = gtk_label_new_with_mnemonic (_("A_liases:"));
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_label_set_yalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 4, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 4, 2, 1);
	gtk_widget_show (widget);

	container = widget;

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (container), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_SHADOW_IN);
	gtk_widget_show (scrolledwindow);

	model = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));
	widget = gtk_tree_view_new_with_model (model);
	gtk_container_add (GTK_CONTAINER (scrolledwindow), widget);
	gtk_widget_show (widget);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

	g_object_unref (model);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), -1, "Aliases",
		renderer, "text", 0, NULL);
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (mail_config_identity_page_aliases_cell_edited_cb), page);
	g_signal_connect (
		renderer, "editing-canceled",
		G_CALLBACK (mail_config_identity_page_aliases_cell_editing_canceled_cb), page);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	gtk_tree_view_column_set_expand (gtk_tree_view_get_column (GTK_TREE_VIEW (widget), 0), TRUE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (mail_config_identity_page_aliases_selection_changed_cb), page);

	page->priv->aliases_treeview = widget;

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		NULL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_dialog_button_new_with_icon ("list-add", _("A_dd"));
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (mail_config_identity_page_add_alias_clicked_cb), page);

	page->priv->aliases_add_button = widget;

	widget = gtk_button_new_with_mnemonic (_("Edi_t"));
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (mail_config_identity_page_edit_alias_clicked_cb), page);

	page->priv->aliases_edit_button = widget;

	widget = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (mail_config_identity_page_remove_alias_clicked_cb), page);

	page->priv->aliases_remove_button = widget;

	mail_config_identity_page_aliases_selection_changed_cb (selection, page);
	mail_config_identity_page_fill_aliases (page, extension);

	g_object_unref (size_group);

	widget = gtk_check_button_new_with_mnemonic (_("_Look up mail server details based on the entered e-mail address"));
	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_END,
		"vexpand", TRUE,
		"active", TRUE,
		NULL);

	e_binding_bind_property (
		page, "show-autodiscover-check",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	page->priv->autodiscover_check = widget;

	gtk_container_add (GTK_CONTAINER (main_box), widget);

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);

	e_extensible_load_extensions (E_EXTENSIBLE (page));
}

static gboolean
mail_config_identity_page_check_complete (EMailConfigPage *page)
{
	EMailConfigIdentityPage *id_page;
	ESource *source;
	ESourceMailIdentity *extension;
	const gchar *extension_name;
	const gchar *name;
	const gchar *address;
	const gchar *reply_to;
	const gchar *display_name;
	gboolean correct, complete = TRUE;

	id_page = E_MAIL_CONFIG_IDENTITY_PAGE (page);
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	source = e_mail_config_identity_page_get_identity_source (id_page);
	extension = e_source_get_extension (source, extension_name);

	name = e_source_mail_identity_get_name (extension);
	address = e_source_mail_identity_get_address (extension);
	reply_to = e_source_mail_identity_get_reply_to (extension);

	display_name = e_source_get_display_name (source);

	correct = name != NULL;
	/* This is only a warning, not a blocker */
	/* complete = complete && correct; */

	e_util_set_entry_issue_hint (id_page->priv->name_entry, correct ? NULL : _("Full Name should not be empty"));

	correct = TRUE;

	/* Only enforce when the email address is visible. */
	if (e_mail_config_identity_page_get_show_email_address (id_page)) {
		if (address == NULL) {
			e_util_set_entry_issue_hint (id_page->priv->address_entry, _("Email Address cannot be empty"));
			correct = FALSE;
		}

		if (correct && !mail_config_identity_page_is_email (address)) {
			e_util_set_entry_issue_hint (id_page->priv->address_entry, _("Email Address is not a valid email"));
			correct = FALSE;
		}
	}

	complete = complete && correct;

	if (correct)
		e_util_set_entry_issue_hint (id_page->priv->address_entry, NULL);

	/* A NULL reply_to string is allowed. */
	if (reply_to != NULL && !mail_config_identity_page_is_email (reply_to)) {
		e_util_set_entry_issue_hint (id_page->priv->reply_to_entry, _("Reply To is not a valid email"));
		complete = FALSE;
	} else {
		e_util_set_entry_issue_hint (id_page->priv->reply_to_entry, NULL);
	}

	correct = TRUE;

	/* Only enforce when account information is visible. */
	if (e_mail_config_identity_page_get_show_account_info (id_page)) {
		if (display_name == NULL || *display_name == '\0') {
			e_util_set_entry_issue_hint (id_page->priv->display_name_entry, _("Account Name cannot be empty"));
			correct = FALSE;
		}
	}

	complete = complete && correct;
	if (correct)
		e_util_set_entry_issue_hint (id_page->priv->display_name_entry, NULL);

	return complete;
}

typedef struct _NameEmailPair {
	gchar *name;
	gchar *email;
} NameEmailPair;

static NameEmailPair *
name_email_pair_new (const gchar *name,
		     const gchar *email)
{
	NameEmailPair *nep;

	nep = g_slice_new (NameEmailPair);
	nep->name = g_strdup (name);
	nep->email = g_strdup (email);

	return nep;
}

static void
name_email_pair_free (gpointer ptr)
{
	NameEmailPair *nep = ptr;

	if (nep) {
		g_free (nep->name);
		g_free (nep->email);
		g_slice_free (NameEmailPair, nep);
	}
}

static gint
name_email_pair_compare (gconstpointer ptr1,
			 gconstpointer ptr2)
{
	const NameEmailPair *nep1 = ptr1;
	const NameEmailPair *nep2 = ptr2;
	gint res = 0;

	if (!nep1 || !nep2) {
		if (nep1 == nep2)
			return 0;
		if (!nep1)
			return -1;
		return 1;
	}

	if (nep1->email && nep2->email)
		res = g_utf8_collate (nep1->email, nep2->email);

	if (!res && nep1->name && nep2->name)
		res = g_utf8_collate (nep1->name, nep2->name);

	if (!res && (!nep1->email || !nep2->email)) {
		if (nep1->email == nep2->email)
			res = 0;
		else if (!nep1->email)
			res = -1;
		else
			res = 1;
	}

	return res;
}

static void
mail_config_identity_page_commit_changes (EMailConfigPage *cfg_page,
					  GQueue *source_queue)
{
	EMailConfigIdentityPage *page;
	ESource *identity_source;
	ESourceMailIdentity *identity_extension;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GString *aliases;
	GSList *pairs = NULL, *link;
	gboolean valid;

	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (cfg_page));

	page = E_MAIL_CONFIG_IDENTITY_PAGE (cfg_page);
	identity_source = e_mail_config_identity_page_get_identity_source (page);
	identity_extension = e_source_get_extension (identity_source, E_SOURCE_EXTENSION_MAIL_IDENTITY);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (page->priv->aliases_treeview));
	for (valid = gtk_tree_model_get_iter_first (model, &iter);
	     valid;
	     valid = gtk_tree_model_iter_next (model, &iter)) {
		gchar *raw = NULL;

		gtk_tree_model_get (model, &iter, 0, &raw, -1);

		if (raw && (*g_strstrip (raw))) {
			CamelInternetAddress *inetaddress;
			CamelAddress *address;

			inetaddress = camel_internet_address_new ();
			address = CAMEL_ADDRESS (inetaddress);

			if (camel_address_unformat (address, raw) > 0) {
				gint ii, len = camel_address_length (address);

				for (ii = 0; ii < len; ii++) {
					const gchar *name = NULL, *email = NULL;

					if (camel_internet_address_get (inetaddress, ii, &name, &email))
						pairs = g_slist_prepend (pairs, name_email_pair_new (name, email));
				}
			}

			g_object_unref (inetaddress);
		}

		g_free (raw);
	}

	pairs = g_slist_sort (pairs, name_email_pair_compare);

	aliases = g_string_new ("");

	for (link = pairs; link; link = g_slist_next (link)) {
		NameEmailPair *nep = link->data;

		if (nep) {
			gchar *encoded;

			encoded = camel_internet_address_encode_address (NULL, nep->name, nep->email);
			if (encoded && *encoded) {
				if (aliases->len)
					g_string_append (aliases, ", ");

				g_string_append (aliases, encoded);
			}

			g_free (encoded);
		}
	}

	g_slist_free_full (pairs, name_email_pair_free);

	if (aliases->len) {
		e_source_mail_identity_set_aliases (identity_extension, aliases->str);
	} else {
		e_source_mail_identity_set_aliases (identity_extension, NULL);
	}

	g_string_free (aliases, TRUE);
}

static void
e_mail_config_identity_page_class_init (EMailConfigIdentityPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_identity_page_set_property;
	object_class->get_property = mail_config_identity_page_get_property;
	object_class->dispose = mail_config_identity_page_dispose;
	object_class->constructed = mail_config_identity_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Registry of data sources",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_SOURCE,
		g_param_spec_object (
			"identity-source",
			"Identity Source",
			"Mail identity source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_ACCOUNT_INFO,
		g_param_spec_boolean (
			"show-account-info",
			"Show Account Info",
			"Show the \"Account Information\" section",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_EMAIL_ADDRESS,
		g_param_spec_boolean (
			"show-email-address",
			"Show Email Address",
			"Show the \"Email Address\" field",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_INSTRUCTIONS,
		g_param_spec_boolean (
			"show-instructions",
			"Show Instructions",
			"Show helpful instructions",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_SIGNATURES,
		g_param_spec_boolean (
			"show-signatures",
			"Show Signatures",
			"Show mail signature options",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_AUTODISCOVER_CHECK,
		g_param_spec_boolean (
			"show-autodiscover-check",
			"Show Autodiscover Check",
			"Show check button to allow autodiscover based on Email Address",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_identity_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Identity");
	iface->sort_order = E_MAIL_CONFIG_IDENTITY_PAGE_SORT_ORDER;
	iface->check_complete = mail_config_identity_page_check_complete;
	iface->commit_changes = mail_config_identity_page_commit_changes;
}

static void
e_mail_config_identity_page_init (EMailConfigIdentityPage *page)
{
	page->priv = e_mail_config_identity_page_get_instance_private (page);
}

EMailConfigPage *
e_mail_config_identity_page_new (ESourceRegistry *registry,
                                 ESource *identity_source)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (E_IS_SOURCE (identity_source), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_IDENTITY_PAGE,
		"registry", registry,
		"identity-source", identity_source,
		NULL);
}

ESourceRegistry *
e_mail_config_identity_page_get_registry (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), NULL);

	return page->priv->registry;
}

ESource *
e_mail_config_identity_page_get_identity_source (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), NULL);

	return page->priv->identity_source;
}

gboolean
e_mail_config_identity_page_get_show_account_info (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_account_info;
}

void
e_mail_config_identity_page_set_show_account_info (EMailConfigIdentityPage *page,
                                                   gboolean show_account_info)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if (page->priv->show_account_info == show_account_info)
		return;

	page->priv->show_account_info = show_account_info;

	g_object_notify (G_OBJECT (page), "show-account-info");
}

gboolean
e_mail_config_identity_page_get_show_email_address (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_email_address;
}

void
e_mail_config_identity_page_set_show_email_address (EMailConfigIdentityPage *page,
                                                    gboolean show_email_address)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if (page->priv->show_email_address == show_email_address)
		return;

	page->priv->show_email_address = show_email_address;

	g_object_notify (G_OBJECT (page), "show-email-address");
}

gboolean
e_mail_config_identity_page_get_show_instructions (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_instructions;
}

void
e_mail_config_identity_page_set_show_instructions (EMailConfigIdentityPage *page,
                                                   gboolean show_instructions)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if (page->priv->show_instructions == show_instructions)
		return;

	page->priv->show_instructions = show_instructions;

	g_object_notify (G_OBJECT (page), "show-instructions");
}

gboolean
e_mail_config_identity_page_get_show_signatures (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_signatures;
}

void
e_mail_config_identity_page_set_show_signatures (EMailConfigIdentityPage *page,
                                                 gboolean show_signatures)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if (page->priv->show_signatures == show_signatures)
		return;

	page->priv->show_signatures = show_signatures;

	g_object_notify (G_OBJECT (page), "show-signatures");
}

void
e_mail_config_identity_page_set_show_autodiscover_check (EMailConfigIdentityPage *page,
							 gboolean show_autodiscover)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page));

	if ((page->priv->show_autodiscover_check ? 1 : 0) == (show_autodiscover ? 1 : 0))
		return;

	page->priv->show_autodiscover_check = show_autodiscover;

	g_object_notify (G_OBJECT (page), "show-autodiscover-check");
}

gboolean
e_mail_config_identity_page_get_show_autodiscover_check (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);

	return page->priv->show_autodiscover_check;
}

GtkWidget *
e_mail_config_identity_page_get_autodiscover_check (EMailConfigIdentityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IDENTITY_PAGE (page), NULL);

	return page->priv->autodiscover_check;
}
