/* e-name-selector-dialog.c - Dialog that lets user pick EDestinations.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Author: Hans Petter Jansson <hpj@novell.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>

#include <libebook/libebook.h>
#include <libebackend/libebackend.h>

#include "e-client-combo-box.h"
#include "e-destination-store.h"
#include "e-contact-store.h"
#include "e-name-selector-dialog.h"
#include "e-name-selector-entry.h"

typedef struct {
	gchar        *name;

	GtkGrid      *section_grid;
	GtkLabel     *label;
	GtkButton    *transfer_button;
	GtkButton    *remove_button;
	GtkTreeView  *destination_view;
}
Section;

typedef struct {
	GtkTreeView *view;
	GtkButton   *button;
	ENameSelectorDialog *dlg_ptr;
} SelData;

struct _ENameSelectorDialogPrivate {
	EClientCache *client_cache;
	ENameSelectorModel *name_selector_model;
	GtkTreeModelSort *contact_sort;
	GCancellable *cancellable;

	GtkWidget *client_combo;
	GtkTreeView *contact_view;
	GtkLabel *status_label;
	GtkGrid *destination_vgrid;
	GtkEntry *search_entry;
	GtkSizeGroup *button_size_group;
	GtkWidget *category_combobox;
	GtkWidget *contact_window;

	GArray *sections;

	guint destination_index;
	GSList *user_query_fields;
	GtkSizeGroup *dest_label_size_group;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE
};

static void     search_changed                (ENameSelectorDialog *name_selector_dialog);
static void     source_changed                (ENameSelectorDialog *name_selector_dialog,
					       EClientComboBox *combo_box);
static void     transfer_button_clicked       (ENameSelectorDialog *name_selector_dialog, GtkButton *transfer_button);
static void     contact_selection_changed     (ENameSelectorDialog *name_selector_dialog);
static void     setup_name_selector_model     (ENameSelectorDialog *name_selector_dialog);
static void     shutdown_name_selector_model  (ENameSelectorDialog *name_selector_dialog);
static void     contact_activated             (ENameSelectorDialog *name_selector_dialog, GtkTreePath *path);
static void     destination_activated         (ENameSelectorDialog *name_selector_dialog, GtkTreePath *path,
					       GtkTreeViewColumn *column, GtkTreeView *tree_view);
static gboolean destination_key_press         (ENameSelectorDialog *name_selector_dialog, GdkEventKey *event, GtkTreeView *tree_view);
static void remove_button_clicked (GtkButton *button, SelData *data);
static void     remove_books                  (ENameSelectorDialog *name_selector_dialog);
static void     contact_column_formatter      (GtkTreeViewColumn *column, GtkCellRenderer *cell,
					       GtkTreeModel *model, GtkTreeIter *iter,
					       ENameSelectorDialog *name_selector_dialog);
static void     destination_column_formatter  (GtkTreeViewColumn *column, GtkCellRenderer *cell,
					       GtkTreeModel *model, GtkTreeIter *iter,
					       ENameSelectorDialog *name_selector_dialog);

/* ------------------ *
 * Class/object setup *
 * ------------------ */

G_DEFINE_TYPE_WITH_CODE (ENameSelectorDialog, e_name_selector_dialog, GTK_TYPE_DIALOG,
	G_ADD_PRIVATE (ENameSelectorDialog)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
name_selector_dialog_populate_categories (ENameSelectorDialog *name_selector_dialog)
{
	GtkWidget *combo_box;
	GList *category_list, *iter;

	/* "Any Category" is preloaded. */
	combo_box = name_selector_dialog->priv->category_combobox;
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box)) == -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);

	/* Categories are already sorted. */
	category_list = e_categories_dup_list ();
	for (iter = category_list; iter != NULL; iter = iter->next) {
		/* Only add user-visible categories. */
		if (!e_categories_is_searchable (iter->data))
			continue;

		gtk_combo_box_text_append_text (
			GTK_COMBO_BOX_TEXT (combo_box), iter->data);
	}

	g_list_free_full (category_list, g_free);

	g_signal_connect_swapped (
		combo_box, "changed",
		G_CALLBACK (search_changed), name_selector_dialog);
}

static void
name_selector_dialog_set_client_cache (ENameSelectorDialog *name_selector_dialog,
                                       EClientCache *client_cache)
{
	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (name_selector_dialog->priv->client_cache == NULL);

	name_selector_dialog->priv->client_cache = g_object_ref (client_cache);
}

static void
name_selector_dialog_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			name_selector_dialog_set_client_cache (
				E_NAME_SELECTOR_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
name_selector_dialog_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			g_value_take_object (
				value,
				e_name_selector_dialog_ref_client_cache (
				E_NAME_SELECTOR_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
name_selector_dialog_dispose (GObject *object)
{
	ENameSelectorDialog *self = E_NAME_SELECTOR_DIALOG (object);

	remove_books (self);
	shutdown_name_selector_model (self);

	g_clear_object (&self->priv->client_cache);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_name_selector_dialog_parent_class)->dispose (object);
}

static void
name_selector_dialog_finalize (GObject *object)
{
	ENameSelectorDialog *self = E_NAME_SELECTOR_DIALOG (object);

	g_slist_free_full (self->priv->user_query_fields, g_free);

	g_array_free (self->priv->sections, TRUE);
	g_object_unref (self->priv->button_size_group);
	g_object_unref (self->priv->dest_label_size_group);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_name_selector_dialog_parent_class)->finalize (object);
}

static void
name_selector_dialog_constructed (GObject *object)
{
	ENameSelectorDialog *self = E_NAME_SELECTOR_DIALOG (object);
	GtkTreeSelection  *contact_selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *cell_renderer;
	GtkTreeSelection  *selection;
	ESourceRegistry *registry;
	ESource *source;
	gchar *tmp_str;
	GtkWidget *name_selector_grid;
	GtkWidget *show_contacts_label;
	GtkWidget *hgrid;
	GtkWidget *label;
	GtkWidget *show_contacts_grid;
	GtkWidget *AddressBookLabel;
	GtkWidget *label_category;
	GtkWidget *search;
	AtkObject *atko;
	GtkWidget *label_search;
	GtkWidget *source_menu_hgrid;
	GtkWidget *combobox_category;
	GtkWidget *label_contacts;
	GtkWidget *scrolledwindow0;
	GtkWidget *scrolledwindow1;
	AtkRelationSet *tmp_relation_set;
	AtkRelationType tmp_relationship;
	AtkRelation *tmp_relation;
	AtkObject *scrolledwindow1_relation_targets[1];
	GtkWidget *source_tree_view;
	GtkWidget *destination_vgrid;
	GtkWidget *status_message;
	GtkWidget *client_combo;
	const gchar *extension_name;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_name_selector_dialog_parent_class)->constructed (object);

	name_selector_grid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_VERTICAL,
		"column-homogeneous", FALSE,
		"row-spacing", 6,
		NULL);
	gtk_widget_show (name_selector_grid);
	gtk_container_set_border_width (GTK_CONTAINER (name_selector_grid), 0);

	tmp_str = g_strconcat ("<b>", _("Show Contacts"), "</b>", NULL);
	show_contacts_label = gtk_label_new (tmp_str);
	gtk_widget_show (show_contacts_label);
	gtk_container_add (GTK_CONTAINER (name_selector_grid), show_contacts_label);
	gtk_label_set_use_markup (GTK_LABEL (show_contacts_label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (show_contacts_label), 0);
	g_free (tmp_str);

	hgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"row-homogeneous", FALSE,
		"column-spacing", 12,
		NULL);
	gtk_widget_show (hgrid);
	gtk_container_add (GTK_CONTAINER (name_selector_grid), hgrid);

	label = gtk_label_new ("");
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (hgrid), label);

	show_contacts_grid = gtk_grid_new ();
	gtk_widget_show (show_contacts_grid);
	gtk_container_add (GTK_CONTAINER (hgrid), show_contacts_grid);
	g_object_set (
		G_OBJECT (show_contacts_grid),
		"column-spacing", 12,
		"row-spacing", 6,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		NULL);

	AddressBookLabel = gtk_label_new_with_mnemonic (_("Address B_ook:"));
	gtk_widget_show (AddressBookLabel);
	gtk_grid_attach (GTK_GRID (show_contacts_grid), AddressBookLabel, 0, 0, 1, 1);
	gtk_widget_set_halign (AddressBookLabel, GTK_ALIGN_FILL);
	gtk_label_set_justify (GTK_LABEL (AddressBookLabel), GTK_JUSTIFY_CENTER);
	gtk_label_set_xalign (GTK_LABEL (AddressBookLabel), 0);

	label_category = gtk_label_new_with_mnemonic (_("Cat_egory:"));
	gtk_widget_show (label_category);
	gtk_grid_attach (GTK_GRID (show_contacts_grid), label_category, 0, 1, 1, 1);
	gtk_widget_set_halign (label_category, GTK_ALIGN_FILL);
	gtk_label_set_justify (GTK_LABEL (label_category), GTK_JUSTIFY_CENTER);
	gtk_label_set_xalign (GTK_LABEL (label_category), 0);

	hgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"row-homogeneous", FALSE,
		"column-spacing", 12,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (hgrid);
	gtk_grid_attach (GTK_GRID (show_contacts_grid), hgrid, 1, 2, 1, 1);

	search = gtk_entry_new ();
	gtk_widget_show (search);
	gtk_widget_set_hexpand (search, TRUE);
	gtk_widget_set_halign (search, GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (hgrid), search);

	label_search = gtk_label_new_with_mnemonic (_("_Search:"));
	gtk_widget_show (label_search);
	gtk_grid_attach (GTK_GRID (show_contacts_grid), label_search, 0, 2, 1, 1);
	gtk_widget_set_halign (label_search, GTK_ALIGN_FILL);
	gtk_label_set_xalign (GTK_LABEL (label_search), 0);

	source_menu_hgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"row-homogeneous", FALSE,
		"column-spacing", 0,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (source_menu_hgrid);
	gtk_grid_attach (GTK_GRID (show_contacts_grid), source_menu_hgrid, 1, 0, 1, 1);

	combobox_category = gtk_combo_box_text_new ();
	gtk_widget_show (combobox_category);
	g_object_set (
		G_OBJECT (combobox_category),
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_grid_attach (GTK_GRID (show_contacts_grid), combobox_category, 1, 1, 1, 1);
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (combobox_category), _("Any Category"));

	tmp_str = g_strconcat ("<b>", _("Co_ntacts"), "</b>", NULL);
	label_contacts = gtk_label_new_with_mnemonic (tmp_str);
	gtk_widget_show (label_contacts);
	gtk_container_add (GTK_CONTAINER (name_selector_grid), label_contacts);
	gtk_label_set_use_markup (GTK_LABEL (label_contacts), TRUE);
	gtk_label_set_xalign (GTK_LABEL (label_contacts), 0);
	g_free (tmp_str);

	scrolledwindow0 = gtk_scrolled_window_new (NULL, NULL);
	self->priv->contact_window = scrolledwindow0;
	gtk_widget_show (scrolledwindow0);
	gtk_widget_set_vexpand (scrolledwindow0, TRUE);
	gtk_widget_set_valign (scrolledwindow0, GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (name_selector_grid), scrolledwindow0);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolledwindow0),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	hgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"row-homogeneous", FALSE,
		"column-spacing", 12,
		NULL);
	gtk_widget_show (hgrid);
	gtk_container_add (GTK_CONTAINER (scrolledwindow0), hgrid);

	label = gtk_label_new ("");
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (hgrid), label);

	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow1);
	gtk_container_add (GTK_CONTAINER (hgrid), scrolledwindow1);
	gtk_widget_set_hexpand (scrolledwindow1, TRUE);
	gtk_widget_set_halign (scrolledwindow1, GTK_ALIGN_FILL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolledwindow1),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_SHADOW_IN);

	source_tree_view = gtk_tree_view_new ();
	gtk_widget_show (source_tree_view);
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), source_tree_view);
	gtk_tree_view_set_headers_visible (
		GTK_TREE_VIEW (source_tree_view), FALSE);
	gtk_tree_view_set_enable_search (
		GTK_TREE_VIEW (source_tree_view), FALSE);

	destination_vgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_VERTICAL,
		"column-homogeneous", TRUE,
		"row-spacing", 6,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (destination_vgrid);
	gtk_container_add (GTK_CONTAINER (hgrid), destination_vgrid);

	status_message = g_object_new (
		GTK_TYPE_LABEL,
		"margin-top", 3,
		"margin-bottom", 3,
		"use-markup", TRUE,
		"visible", TRUE,
		"xalign", 0.0,
		NULL);
	gtk_container_add (GTK_CONTAINER (name_selector_grid), status_message);

	gtk_label_set_mnemonic_widget (GTK_LABEL (AddressBookLabel), source_menu_hgrid);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label_category), combobox_category);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label_search), search);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label_contacts), source_tree_view);

	atko = gtk_widget_get_accessible (search);
	atk_object_set_name (atko, _("Search"));

	atko = gtk_widget_get_accessible (source_menu_hgrid);
	atk_object_set_name (atko, _("Address Book"));

	atko = gtk_widget_get_accessible (scrolledwindow1);
	atk_object_set_name (atko, _("Contacts"));
	tmp_relation_set = atk_object_ref_relation_set (atko);
	scrolledwindow1_relation_targets[0] = gtk_widget_get_accessible (label_contacts);
	tmp_relationship = atk_relation_type_for_name ("labelled-by");
	tmp_relation = atk_relation_new (scrolledwindow1_relation_targets, 1, tmp_relationship);
	atk_relation_set_add (tmp_relation_set, tmp_relation);
	g_object_unref (G_OBJECT (tmp_relation));
	g_object_unref (G_OBJECT (tmp_relation_set));

	gtk_box_pack_start (
		GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (object))),
		name_selector_grid, TRUE, TRUE, 0);

	/* Store pointers to relevant widgets */

	self->priv->contact_view = GTK_TREE_VIEW (source_tree_view);
	self->priv->status_label = GTK_LABEL (status_message);
	self->priv->destination_vgrid = GTK_GRID (destination_vgrid);
	self->priv->search_entry = GTK_ENTRY (search);
	self->priv->category_combobox = combobox_category;

	/* Create size group for transfer buttons */

	self->priv->button_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* Create size group for destination labels */

	self->priv->dest_label_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* Set up contacts view */

	column = gtk_tree_view_column_new ();
	cell_renderer = GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (
		column, cell_renderer, (GtkTreeCellDataFunc)
		contact_column_formatter, object, NULL);

	selection = gtk_tree_view_get_selection (self->priv->contact_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_view_append_column (self->priv->contact_view, column);
	g_signal_connect_swapped (
		self->priv->contact_view, "row-activated",
		G_CALLBACK (contact_activated), object);

	/* Listen for changes to the contact selection */

	contact_selection = gtk_tree_view_get_selection (self->priv->contact_view);
	g_signal_connect_swapped (
		contact_selection, "changed",
		G_CALLBACK (contact_selection_changed), object);

	/* Set up our data structures */

	self->priv->name_selector_model = e_name_selector_model_new ();
	self->priv->sections = g_array_new (FALSE, FALSE, sizeof (Section));

	setup_name_selector_model (E_NAME_SELECTOR_DIALOG (object));

	/* Create source menu */

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	client_combo = e_client_combo_box_new (
		self->priv->client_cache, extension_name);
	self->priv->client_combo = client_combo;
	g_signal_connect_swapped (
		client_combo, "changed",
		G_CALLBACK (source_changed), object);

	gtk_label_set_mnemonic_widget (
		GTK_LABEL (AddressBookLabel), client_combo);
	gtk_widget_show (client_combo);
	gtk_widget_set_hexpand (client_combo, TRUE);
	gtk_widget_set_halign (client_combo, GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (source_menu_hgrid), client_combo);

	name_selector_dialog_populate_categories (
		E_NAME_SELECTOR_DIALOG (object));

	/* Set up search-as-you-type signal */

	g_signal_connect_swapped (
		search, "changed",
		G_CALLBACK (search_changed), object);

	/* Display initial source */

	registry = e_client_cache_ref_registry (self->priv->client_cache);
	source = e_source_registry_ref_default_address_book (registry);
	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (client_combo), source);
	g_object_unref (source);
	g_object_unref (registry);

	/* Set up dialog defaults */

	gtk_dialog_add_buttons (
		GTK_DIALOG (object),
		_("_Close"), GTK_RESPONSE_CLOSE,
		NULL);

	/* Try to figure out a sane default size for the dialog. We used to hard
	 * code this to 512 so keep using 512 if the screen is big enough,
	 * otherwise use -1 (use as little as possible, use the
	 * GtkScrolledWindow's scrollbars).
	 *
	 * This should allow scrolling on tiny netbook resolutions and let
	 * others see as much of the dialog as possible.
	 *
	 * 600 pixels seems to be a good lower bound resolution to allow room
	 * above or below for other UI (window manager's?)
	 */
	gtk_window_set_default_size (
		GTK_WINDOW (object), 700,
		gdk_screen_height () >= 600 ? 512 : -1);

	gtk_dialog_set_default_response (
		GTK_DIALOG (object), GTK_RESPONSE_CLOSE);
	gtk_window_set_modal (GTK_WINDOW (object), TRUE);
	gtk_window_set_resizable (GTK_WINDOW (object), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (object), 4);
	gtk_window_set_title (
		GTK_WINDOW (object),
		_("Select Contacts from Address Book"));
	gtk_widget_grab_focus (search);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
name_selector_dialog_realize (GtkWidget *widget)
{
	ENameSelectorDialog *name_selector_dialog;
	GtkWidget *client_combo;

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_name_selector_dialog_parent_class)->
		realize (widget);

	name_selector_dialog = E_NAME_SELECTOR_DIALOG (widget);
	client_combo = name_selector_dialog->priv->client_combo;

	source_changed (
		name_selector_dialog,
		E_CLIENT_COMBO_BOX (client_combo));
}

static void
e_name_selector_dialog_class_init (ENameSelectorDialogClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = name_selector_dialog_set_property;
	object_class->get_property = name_selector_dialog_get_property;
	object_class->dispose = name_selector_dialog_dispose;
	object_class->finalize = name_selector_dialog_finalize;
	object_class->constructed = name_selector_dialog_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = name_selector_dialog_realize;

	/**
	 * ENameSelectorDialog:client-cache:
	 *
	 * Cache of shared #EClient instances.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Cache of shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_name_selector_dialog_init (ENameSelectorDialog *name_selector_dialog)
{
	name_selector_dialog->priv = e_name_selector_dialog_get_instance_private (name_selector_dialog);
}

/**
 * e_name_selector_dialog_new:
 * @client_cache: an #EClientCache
 *
 * Creates a new #ENameSelectorDialog.
 *
 * Returns: A new #ENameSelectorDialog.
 **/
ENameSelectorDialog *
e_name_selector_dialog_new (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_new (
		E_TYPE_NAME_SELECTOR_DIALOG,
		"client-cache", client_cache, NULL);
}

/**
 * e_name_selector_dialog_ref_client_cache:
 * @name_selector_dialog: an #ENameSelectorDialog
 *
 * Returns the #EClientCache passed to e_name_selector_dialog_new().
 *
 * The returned #EClientCache is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #EClientCache
 *
 * Since: 3.8
 **/
EClientCache *
e_name_selector_dialog_ref_client_cache (ENameSelectorDialog *name_selector_dialog)
{
	g_return_val_if_fail (
		E_IS_NAME_SELECTOR_DIALOG (name_selector_dialog), NULL);

	return g_object_ref (name_selector_dialog->priv->client_cache);
}

/* --------- *
 * Utilities *
 * --------- */

static gchar *
escape_sexp_string (const gchar *string)
{
	GString *gstring;

	gstring = g_string_new ("");
	e_sexp_encode_string (gstring, string);

	return g_string_free (gstring, FALSE);
}

static void
sort_iter_to_contact_store_iter (ENameSelectorDialog *name_selector_dialog,
                                 GtkTreeIter *iter,
                                 gint *email_n)
{
	ETreeModelGenerator *contact_filter;
	GtkTreeIter          child_iter;
	gint                 email_n_local;

	contact_filter = e_name_selector_model_peek_contact_filter (
		name_selector_dialog->priv->name_selector_model);

	gtk_tree_model_sort_convert_iter_to_child_iter (
		name_selector_dialog->priv->contact_sort, &child_iter, iter);
	e_tree_model_generator_convert_iter_to_child_iter (
		contact_filter, iter, &email_n_local, &child_iter);

	if (email_n)
		*email_n = email_n_local;
}

static void
add_destination (ENameSelectorModel *name_selector_model,
                 EDestinationStore *destination_store,
                 EContact *contact,
                 gint email_n,
                 EBookClient *client)
{
	EDestination *destination;
	GList *email_list, *nth;

	/* get the correct index of an email in the contact */
	email_list = e_name_selector_model_get_contact_emails_without_used (name_selector_model, contact, FALSE);
	while (nth = g_list_nth (email_list, email_n), nth && nth->data == NULL) {
		email_n++;
	}
	e_name_selector_model_free_emails_list (email_list);

	/* Transfer (actually, copy into a destination and let the model filter out the
	 * source automatically) */

	destination = e_destination_new ();
	e_destination_set_contact (destination, contact, email_n);
	if (client)
		e_destination_set_client (destination, client);
	e_destination_store_append_destination (destination_store, destination);
	g_object_unref (destination);
}

static void
disable_sort (ENameSelectorDialog *dialog)
{
	g_clear_object (&dialog->priv->contact_sort);

	gtk_tree_view_set_model (
		dialog->priv->contact_view,
		NULL);
}

static void
enable_sort (ENameSelectorDialog *dialog)
{
	ETreeModelGenerator *contact_filter;

	/* Get contact store and its filter wrapper */
	contact_filter = e_name_selector_model_peek_contact_filter (
		dialog->priv->name_selector_model);

	/* Create sorting model on top of filter, assign it to view */
	if (!dialog->priv->contact_sort) {
		dialog->priv->contact_sort = GTK_TREE_MODEL_SORT (
			gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (contact_filter)));

		/* sort on full name as we display full name in name selector dialog */
		gtk_tree_sortable_set_sort_column_id (
			GTK_TREE_SORTABLE (dialog->priv->contact_sort),
			E_CONTACT_FULL_NAME, GTK_SORT_ASCENDING);
	}

	gtk_tree_view_set_model (
		dialog->priv->contact_view,
		GTK_TREE_MODEL (dialog->priv->contact_sort));
}

static void
remove_books (ENameSelectorDialog *name_selector_dialog)
{
	EContactStore *contact_store;
	GSList        *clients, *l;

	if (!name_selector_dialog->priv->name_selector_model)
		return;

	contact_store = e_name_selector_model_peek_contact_store (
		name_selector_dialog->priv->name_selector_model);

	/* Remove books (should be just one) being viewed */
	clients = e_contact_store_get_clients (contact_store);
	for (l = clients; l; l = g_slist_next (l)) {
		EBookClient *client = l->data;
		e_contact_store_remove_client (contact_store, client);
	}
	g_slist_free (clients);

	/* See if we have a book pending; stop loading it if so */
	if (name_selector_dialog->priv->cancellable != NULL) {
		g_cancellable_cancel (name_selector_dialog->priv->cancellable);
		g_object_unref (name_selector_dialog->priv->cancellable);
		name_selector_dialog->priv->cancellable = NULL;
	}

	disable_sort (name_selector_dialog);
}


/* ------------------ *
 * Section management *
 * ------------------ */

static gint
find_section_by_transfer_button (ENameSelectorDialog *name_selector_dialog,
                                 GtkButton *transfer_button)
{
	gint i;

	for (i = 0; i < name_selector_dialog->priv->sections->len; i++) {
		Section *section = &g_array_index (
			name_selector_dialog->priv->sections, Section, i);

		if (section->transfer_button == transfer_button)
			return i;
	}

	return -1;
}

static gint
find_section_by_tree_view (ENameSelectorDialog *name_selector_dialog,
                           GtkTreeView *tree_view)
{
	gint i;

	for (i = 0; i < name_selector_dialog->priv->sections->len; i++) {
		Section *section = &g_array_index (
			name_selector_dialog->priv->sections, Section, i);

		if (section->destination_view == tree_view)
			return i;
	}

	return -1;
}

static gint
find_section_by_name (ENameSelectorDialog *name_selector_dialog,
                      const gchar *name)
{
	gint i;

	for (i = 0; i < name_selector_dialog->priv->sections->len; i++) {
		Section *section = &g_array_index (
			name_selector_dialog->priv->sections, Section, i);

		if (!strcmp (name, section->name))
			return i;
	}

	return -1;
}

static void
selection_changed (GtkTreeSelection *selection,
                   SelData *data)
{
	GtkTreeSelection *contact_selection;
	gboolean          have_selection = FALSE;

	contact_selection = gtk_tree_view_get_selection (data->view);
	if (gtk_tree_selection_count_selected_rows (contact_selection) > 0)
		have_selection = TRUE;
	gtk_widget_set_sensitive (GTK_WIDGET (data->button), have_selection);
}

static GtkTreeView *
make_tree_view_for_section (ENameSelectorDialog *name_selector_dialog,
                            EDestinationStore *destination_store)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *cell_renderer;

	tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());

	column = gtk_tree_view_column_new ();
	cell_renderer = GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (
		column, cell_renderer,
		(GtkTreeCellDataFunc) destination_column_formatter,
		name_selector_dialog, NULL);
	gtk_tree_view_append_column (tree_view, column);
	gtk_tree_view_set_headers_visible (tree_view, FALSE);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (destination_store));

	return tree_view;
}

static void
setup_section_button (ENameSelectorDialog *name_selector_dialog,
                      GtkButton *button,
                      const gchar *label_text,
                      const gchar *icon_name,
                      gboolean icon_before_label)
{
	GtkWidget *hgrid;
	GtkWidget *label;
	GtkWidget *image;

	gtk_size_group_add_widget (
		name_selector_dialog->priv->button_size_group,
		GTK_WIDGET (button));

	hgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"row-homogeneous", FALSE,
		"column-spacing", 2,
		NULL);
	gtk_widget_show (hgrid);
	gtk_container_add (GTK_CONTAINER (button), hgrid);

	label = gtk_label_new_with_mnemonic (label_text);
	gtk_widget_show (label);

	image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);

	if (icon_before_label) {
		gtk_container_add (GTK_CONTAINER (hgrid), image);
		gtk_container_add (GTK_CONTAINER (hgrid), label);
	} else {
		gtk_container_add (GTK_CONTAINER (hgrid), label);
		gtk_container_add (GTK_CONTAINER (hgrid), image);
	}
}

static gint
add_section (ENameSelectorDialog *name_selector_dialog,
             const gchar *name,
             const gchar *pretty_name,
             EDestinationStore *destination_store)
{
	Section            section;
	GtkWidget	  *vgrid;
	GtkWidget	  *scrollwin;
	SelData		  *data;
	GtkTreeSelection  *selection;
	gchar		  *text;
	GtkWidget         *hgrid;

	g_return_val_if_fail (name != NULL, -1);
	g_return_val_if_fail (pretty_name != NULL, -1);
	g_return_val_if_fail (E_IS_DESTINATION_STORE (destination_store), -1);

	memset (&section, 0, sizeof (Section));

	section.name = g_strdup (name);
	section.section_grid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"row-homogeneous", FALSE,
		"column-spacing", 12,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	section.label = GTK_LABEL (gtk_label_new_with_mnemonic (pretty_name));
	section.transfer_button = GTK_BUTTON (gtk_button_new ());
	section.remove_button = GTK_BUTTON (gtk_button_new ());
	section.destination_view = make_tree_view_for_section (name_selector_dialog, destination_store);

	gtk_label_set_mnemonic_widget (GTK_LABEL (section.label), GTK_WIDGET (section.destination_view));

	if (pango_parse_markup (pretty_name, -1, '_', NULL,
				&text, NULL, NULL))  {
		atk_object_set_name (gtk_widget_get_accessible (
					GTK_WIDGET (section.destination_view)), text);
		atk_object_set_description (gtk_widget_get_accessible (GTK_WIDGET (section.transfer_button)), text);
		atk_object_set_description (gtk_widget_get_accessible (GTK_WIDGET (section.remove_button)), text);
		g_free (text);
	}

	/* Set up transfer button */
	g_signal_connect_swapped (
		section.transfer_button, "clicked",
		G_CALLBACK (transfer_button_clicked), name_selector_dialog);

	/*data for the remove callback*/
	data = g_malloc0 (sizeof (SelData));
	data->view = section.destination_view;
	data->dlg_ptr = name_selector_dialog;

	/*Associate to an object destroy so that it gets freed*/
	g_object_set_data_full ((GObject *) section.destination_view, "sel-remove-data", data, g_free);

	g_signal_connect (
		section.remove_button, "clicked",
		G_CALLBACK (remove_button_clicked), data);

	vgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_VERTICAL,
		"column-homogeneous", TRUE,
		"row-spacing", 6,
		NULL);

	gtk_container_add (GTK_CONTAINER (section.section_grid), vgrid);

	/* "Add" button */
	gtk_container_add (GTK_CONTAINER (vgrid), GTK_WIDGET (section.transfer_button));
	setup_section_button (name_selector_dialog, section.transfer_button, _("_Add"), "go-next", FALSE);

	/* "Remove" button */
	gtk_container_add (GTK_CONTAINER (vgrid), GTK_WIDGET (section.remove_button));
	setup_section_button (name_selector_dialog, section.remove_button, _("_Remove"), "go-previous", TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (section.remove_button), FALSE);

	/* hgrid for label and scrolled window. This is a separate hgrid, instead
	 * of just using the section.section_grid directly, as it has a different
	 * spacing.
	 */

	hgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"row-homogeneous", FALSE,
		"column-spacing", 6,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_container_add (GTK_CONTAINER (section.section_grid), hgrid);

	/* Title label */

	gtk_size_group_add_widget (name_selector_dialog->priv->dest_label_size_group, GTK_WIDGET (section.label));

	gtk_label_set_xalign (GTK_LABEL (section.label), 0);
	gtk_label_set_yalign (GTK_LABEL (section.label), 0);
	gtk_container_add (GTK_CONTAINER (hgrid), GTK_WIDGET (section.label));

	/* Treeview in a scrolled window */
	scrollwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (hgrid), scrollwin);
	gtk_widget_set_hexpand (scrollwin, TRUE);
	gtk_widget_set_halign (scrollwin, GTK_ALIGN_FILL);
	gtk_widget_set_vexpand (scrollwin, TRUE);
	gtk_widget_set_valign (scrollwin, GTK_ALIGN_FILL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrollwin), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scrollwin), GTK_WIDGET (section.destination_view));

	/*data for 'changed' callback*/
	data = g_malloc0 (sizeof (SelData));
	data->view = section.destination_view;
	data->button = section.remove_button;
	g_object_set_data_full ((GObject *) section.destination_view, "sel-change-data", data, g_free);
	selection = gtk_tree_view_get_selection (section.destination_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (
		selection, "changed",
		G_CALLBACK (selection_changed), data);

	g_signal_connect_swapped (
		section.destination_view, "row-activated",
		G_CALLBACK (destination_activated), name_selector_dialog);
	g_signal_connect_swapped (
		section.destination_view, "key-press-event",
		G_CALLBACK (destination_key_press), name_selector_dialog);

	/* Done! */

	gtk_widget_show_all (GTK_WIDGET (section.section_grid));

	/* Pack this section's box into the dialog */
	gtk_container_add (GTK_CONTAINER (name_selector_dialog->priv->destination_vgrid), GTK_WIDGET (section.section_grid));
	g_object_set (
		G_OBJECT (section.section_grid),
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);

	g_array_append_val (name_selector_dialog->priv->sections, section);

	/* Make sure UI is consistent */
	contact_selection_changed (name_selector_dialog);

	return name_selector_dialog->priv->sections->len - 1;
}

static void
free_section (ENameSelectorDialog *name_selector_dialog,
              gint n)
{
	Section *section;

	g_return_if_fail (n >= 0);
	g_return_if_fail (n < name_selector_dialog->priv->sections->len);

	section = &g_array_index (
		name_selector_dialog->priv->sections, Section, n);

	g_free (section->name);
	gtk_widget_destroy (GTK_WIDGET (section->section_grid));
}

static void
model_section_added (ENameSelectorDialog *name_selector_dialog,
                     const gchar *name)
{
	gchar             *pretty_name;
	EDestinationStore *destination_store;

	e_name_selector_model_peek_section (
		name_selector_dialog->priv->name_selector_model,
		name, &pretty_name, &destination_store);
	add_section (name_selector_dialog, name, pretty_name, destination_store);
	g_free (pretty_name);
}

static void
model_section_removed (ENameSelectorDialog *name_selector_dialog,
                       const gchar *name)
{
	gint section_index;

	section_index = find_section_by_name (name_selector_dialog, name);
	if (section_index < 0) {
		g_warn_if_reached ();
		return;
	}

	free_section (name_selector_dialog, section_index);
	g_array_remove_index (
		name_selector_dialog->priv->sections, section_index);
}

/* -------------------- *
 * Addressbook selector *
 * -------------------- */

static void
view_progress (EBookClientView *view,
               guint percent,
               const gchar *message,
               ENameSelectorDialog *dialog)
{
	if (message == NULL)
		gtk_label_set_text (dialog->priv->status_label, "");
	else
		gtk_label_set_text (dialog->priv->status_label, message);
}

static void
view_complete (EBookClientView *view,
               const GError *error,
               ENameSelectorDialog *dialog)
{
	view_progress (view, -1, NULL, dialog);

	enable_sort (dialog);
}

static void
start_client_view_cb (EContactStore *store,
                      EBookClientView *client_view,
                      ENameSelectorDialog *name_selector_dialog)
{
	g_signal_connect (
		client_view, "progress",
		G_CALLBACK (view_progress), name_selector_dialog);

	g_signal_connect (
		client_view, "complete",
		G_CALLBACK (view_complete), name_selector_dialog);
}

static void
stop_client_view_cb (EContactStore *store,
                     EBookClientView *client_view,
                     ENameSelectorDialog *name_selector_dialog)
{
	g_signal_handlers_disconnect_by_func (client_view, view_progress, name_selector_dialog);
	g_signal_handlers_disconnect_by_func (client_view, view_complete, name_selector_dialog);
}

static void
start_update_cb (EContactStore *store,
                 EBookClientView *client_view,
                 ENameSelectorDialog *name_selector_dialog)
{
	disable_sort (name_selector_dialog);
}

static void
stop_update_cb (EContactStore *store,
                EBookClientView *client_view,
                ENameSelectorDialog *name_selector_dialog)
{
	enable_sort (name_selector_dialog);
}

static void
name_selector_dialog_get_client_cb (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
	ENameSelectorDialog *name_selector_dialog = user_data;
	EClient *client;
	EBookClient *book_client;
	EContactStore *store;
	ENameSelectorModel *model;
	GError *error = NULL;

	client = e_client_combo_box_get_client_finish (
		E_CLIENT_COMBO_BOX (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		goto exit;
	}

	if (error != NULL) {
		gtk_label_set_text (
			name_selector_dialog->priv->status_label,
			error->message);
		g_error_free (error);
		goto exit;
	}

	book_client = E_BOOK_CLIENT (client);
	if (!book_client) {
		g_warn_if_fail (book_client != NULL);
		goto exit;
	}

	model = name_selector_dialog->priv->name_selector_model;
	store = e_name_selector_model_peek_contact_store (model);
	e_contact_store_add_client (store, book_client);
	g_object_unref (book_client);

 exit:
	g_object_unref (name_selector_dialog);
}

static void
source_changed (ENameSelectorDialog *name_selector_dialog,
                EClientComboBox *combo_box)
{
	GCancellable *cancellable;
	ESource *source;

	source = e_source_combo_box_ref_active (E_SOURCE_COMBO_BOX (combo_box));

	/* Remove any previous books being shown or loaded */
	remove_books (name_selector_dialog);

	if (source == NULL)
		return;

	cancellable = g_cancellable_new ();
	name_selector_dialog->priv->cancellable = cancellable;

	/* Connect to the selected source. */
	e_client_combo_box_get_client (
		combo_box, source, cancellable,
		name_selector_dialog_get_client_cb,
		g_object_ref (name_selector_dialog));

	g_object_unref (source);
}

/* --------------- *
 * Other UI events *
 * --------------- */

static void
search_changed (ENameSelectorDialog *name_selector_dialog)
{
	EContactStore *contact_store;
	EBookQuery    *book_query;
	GtkWidget     *combo_box;
	const gchar   *text;
	gchar         *text_escaped;
	gchar         *query_string;
	gchar         *category;
	gchar         *category_escaped;

	combo_box = name_selector_dialog->priv->category_combobox;
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box)) == -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);

	category = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box));
	category_escaped = escape_sexp_string (category);

	text = gtk_entry_get_text (name_selector_dialog->priv->search_entry);
	text_escaped = escape_sexp_string (text);

	if (g_strcmp0 (category, _("Any Category")) == 0)
		query_string = g_strdup_printf (
			"(or (contains \"file_as\" %s) "
			"    (contains \"full_name\" %s) "
			"    (contains \"email\" %s) "
			"    (contains \"nickname\" %s)))",
			text_escaped, text_escaped,
			text_escaped, text_escaped);
	else
		query_string = g_strdup_printf (
			"(and (is \"category_list\" %s) "
			"(or (contains \"file_as\" %s) "
			"    (contains \"full_name\" %s) "
			"    (contains \"email\" %s) "
			"    (contains \"nickname\" %s)))",
			category_escaped, text_escaped, text_escaped,
			text_escaped, text_escaped);

	book_query = e_book_query_from_string (query_string);

	contact_store = e_name_selector_model_peek_contact_store (
		name_selector_dialog->priv->name_selector_model);
	e_contact_store_set_query (contact_store, book_query);
	e_book_query_unref (book_query);

	g_free (query_string);
	g_free (text_escaped);
	g_free (category_escaped);
	g_free (category);
}

static void
contact_selection_changed (ENameSelectorDialog *name_selector_dialog)
{
	GtkTreeSelection *contact_selection;
	gboolean          have_selection = FALSE;
	gint              i;

	contact_selection = gtk_tree_view_get_selection (
		name_selector_dialog->priv->contact_view);
	if (gtk_tree_selection_count_selected_rows (contact_selection))
		have_selection = TRUE;

	for (i = 0; i < name_selector_dialog->priv->sections->len; i++) {
		Section *section = &g_array_index (
			name_selector_dialog->priv->sections, Section, i);
		gtk_widget_set_sensitive (GTK_WIDGET (section->transfer_button), have_selection);
	}
}

static void
contact_activated (ENameSelectorDialog *name_selector_dialog,
                   GtkTreePath *path)
{
	EContactStore     *contact_store;
	EDestinationStore *destination_store;
	EContact          *contact;
	GtkTreeIter       iter;
	Section           *section;
	gint               email_n;

	/* When a contact is activated, we transfer it to the first destination on our list */

	contact_store = e_name_selector_model_peek_contact_store (
		name_selector_dialog->priv->name_selector_model);

	/* If we have no sections, we can't transfer */
	if (name_selector_dialog->priv->sections->len == 0)
		return;

	/* Get the contact to be transferred */

	if (!gtk_tree_model_get_iter (
		GTK_TREE_MODEL (name_selector_dialog->priv->contact_sort),
		&iter, path))
		g_return_if_reached ();

	sort_iter_to_contact_store_iter (name_selector_dialog, &iter, &email_n);

	contact = e_contact_store_get_contact (contact_store, &iter);
	if (!contact) {
		g_warning ("ENameSelectorDialog could not get selected contact!");
		return;
	}

	section = &g_array_index (
		name_selector_dialog->priv->sections,
		Section, name_selector_dialog->priv->destination_index);
	if (!e_name_selector_model_peek_section (
		name_selector_dialog->priv->name_selector_model,
		section->name, NULL, &destination_store)) {
		g_warning ("ENameSelectorDialog has a section unknown to the model!");
		return;
	}

	add_destination (
		name_selector_dialog->priv->name_selector_model,
		destination_store, contact, email_n,
		e_contact_store_get_client (contact_store, &iter));
}

static void
destination_activated (ENameSelectorDialog *name_selector_dialog,
                       GtkTreePath *path,
                       GtkTreeViewColumn *column,
                       GtkTreeView *tree_view)
{
	gint               section_index;
	EDestinationStore *destination_store;
	EDestination      *destination;
	Section           *section;
	GtkTreeIter        iter;

	/* When a destination is activated, we remove it from the section */

	section_index = find_section_by_tree_view (
		name_selector_dialog, tree_view);
	if (section_index < 0) {
		g_warning ("ENameSelectorDialog got activation from unknown view!");
		return;
	}

	section = &g_array_index (
		name_selector_dialog->priv->sections, Section, section_index);
	if (!e_name_selector_model_peek_section (
		name_selector_dialog->priv->name_selector_model,
		section->name, NULL, &destination_store)) {
		g_warning ("ENameSelectorDialog has a section unknown to the model!");
		return;
	}

	if (!gtk_tree_model_get_iter (
		GTK_TREE_MODEL (destination_store), &iter, path))
		g_return_if_reached ();

	destination = e_destination_store_get_destination (
		destination_store, &iter);
	g_return_if_fail (destination);

	e_destination_store_remove_destination (
		destination_store, destination);
}

static gboolean
remove_selection (ENameSelectorDialog *name_selector_dialog,
                  GtkTreeView *tree_view)
{
	gint               section_index;
	EDestinationStore *destination_store;
	EDestination      *destination;
	Section           *section;
	GtkTreeSelection  *selection;
	GList		  *rows, *l;
	gboolean res = TRUE;

	section_index = find_section_by_tree_view (
		name_selector_dialog, tree_view);
	if (section_index < 0) {
		g_warning ("ENameSelectorDialog got key press from unknown view!");
		return FALSE;
	}

	section = &g_array_index (
		name_selector_dialog->priv->sections, Section, section_index);
	if (!e_name_selector_model_peek_section (
		name_selector_dialog->priv->name_selector_model,
		section->name, NULL, &destination_store)) {
		g_warning ("ENameSelectorDialog has a section unknown to the model!");
		return FALSE;
	}

	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_count_selected_rows (selection)) {
		g_warning ("ENameSelectorDialog remove button clicked, but no selection!");
		return FALSE;
	}

	rows = gtk_tree_selection_get_selected_rows (selection, NULL);
	rows = g_list_reverse (rows);

	for (l = rows; l; l = g_list_next (l)) {
		GtkTreeIter iter;
		GtkTreePath *path = l->data;

		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (destination_store), &iter, path)) {
			g_warn_if_reached ();
			res = FALSE;
			break;
		}

		destination = e_destination_store_get_destination (destination_store, &iter);
		if (!destination) {
			g_warn_if_reached ();
			res = FALSE;
			break;
		}

		e_destination_store_remove_destination (
			destination_store, destination);
	}

	g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);

	return res;
}

static void
remove_button_clicked (GtkButton *button,
                       SelData *data)
{
	GtkTreeView *view;
	ENameSelectorDialog *name_selector_dialog;

	view = data->view;
	name_selector_dialog = data->dlg_ptr;
	remove_selection (name_selector_dialog, view);
}

static gboolean
destination_key_press (ENameSelectorDialog *name_selector_dialog,
                       GdkEventKey *event,
                       GtkTreeView *tree_view)
{

	/* we only care about DEL key */
	if (event->keyval != GDK_KEY_Delete)
		return FALSE;
	return remove_selection (name_selector_dialog, tree_view);

}

static void
transfer_button_clicked (ENameSelectorDialog *name_selector_dialog,
                         GtkButton *transfer_button)
{
	EContactStore     *contact_store;
	EDestinationStore *destination_store;
	GtkTreeSelection  *selection;
	EContact          *contact;
	gint               section_index;
	Section           *section;
	gint               email_n;
	GList		  *rows, *l;

	/* Get the contact to be transferred */

	contact_store = e_name_selector_model_peek_contact_store (
		name_selector_dialog->priv->name_selector_model);
	selection = gtk_tree_view_get_selection (
		name_selector_dialog->priv->contact_view);

	if (!gtk_tree_selection_count_selected_rows (selection)) {
		g_warning ("ENameSelectorDialog transfer button clicked, but no selection!");
		return;
	}

	/* Get the target section */
	section_index = find_section_by_transfer_button (
		name_selector_dialog, transfer_button);
	if (section_index < 0) {
		g_warning ("ENameSelectorDialog got click from unknown button!");
		return;
	}

	section = &g_array_index (
		name_selector_dialog->priv->sections, Section, section_index);
	if (!e_name_selector_model_peek_section (
		name_selector_dialog->priv->name_selector_model,
		section->name, NULL, &destination_store)) {
		g_warning ("ENameSelectorDialog has a section unknown to the model!");
		return;
	}

	rows = gtk_tree_selection_get_selected_rows (selection, NULL);
	rows = g_list_reverse (rows);

	for (l = rows; l; l = g_list_next (l)) {
		GtkTreeIter iter;
		GtkTreePath *path = l->data;

		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (name_selector_dialog->priv->contact_sort), &iter, path))
			break;

		sort_iter_to_contact_store_iter (name_selector_dialog, &iter, &email_n);

		contact = e_contact_store_get_contact (contact_store, &iter);
		if (!contact) {
			g_warning ("ENameSelectorDialog could not get selected contact!");
			break;
		}

		add_destination (
			name_selector_dialog->priv->name_selector_model,
			destination_store, contact, email_n,
			e_contact_store_get_client (contact_store, &iter));
	}

	g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);
}

/* --------------------- *
 * Main model management *
 * --------------------- */

static void
setup_name_selector_model (ENameSelectorDialog *name_selector_dialog)
{
	EContactStore       *contact_store;
	GList               *new_sections;
	GList               *l;

	/* Create new destination sections in UI */

	new_sections = e_name_selector_model_list_sections (
		name_selector_dialog->priv->name_selector_model);

	for (l = new_sections; l; l = g_list_next (l)) {
		gchar             *name = l->data;
		gchar             *pretty_name;
		EDestinationStore *destination_store;

		e_name_selector_model_peek_section (
			name_selector_dialog->priv->name_selector_model,
			name, &pretty_name, &destination_store);

		add_section (name_selector_dialog, name, pretty_name, destination_store);

		g_free (pretty_name);
		g_free (name);
	}

	g_list_free (new_sections);

	/* Connect to section add/remove signals */

	g_signal_connect_swapped (
		name_selector_dialog->priv->name_selector_model, "section-added",
		G_CALLBACK (model_section_added), name_selector_dialog);
	g_signal_connect_swapped (
		name_selector_dialog->priv->name_selector_model, "section-removed",
		G_CALLBACK (model_section_removed), name_selector_dialog);

	contact_store = e_name_selector_model_peek_contact_store (name_selector_dialog->priv->name_selector_model);
	if (contact_store) {
		g_signal_connect (contact_store, "start-client-view", G_CALLBACK (start_client_view_cb), name_selector_dialog);
		g_signal_connect (contact_store, "stop-client-view", G_CALLBACK (stop_client_view_cb), name_selector_dialog);
		g_signal_connect (contact_store, "start-update", G_CALLBACK (start_update_cb), name_selector_dialog);
		g_signal_connect (contact_store, "stop-update", G_CALLBACK (stop_update_cb), name_selector_dialog);
	}

	/* Make sure UI is consistent */

	search_changed (name_selector_dialog);
	contact_selection_changed (name_selector_dialog);
}

static void
shutdown_name_selector_model (ENameSelectorDialog *name_selector_dialog)
{
	gint i;

	/* Rid UI of previous destination sections */

	for (i = 0; i < name_selector_dialog->priv->sections->len; i++)
		free_section (name_selector_dialog, i);

	g_array_set_size (name_selector_dialog->priv->sections, 0);

	/* Free sorting model */

	g_clear_object (&name_selector_dialog->priv->contact_sort);

	/* Free backend model */

	if (name_selector_dialog->priv->name_selector_model) {
		EContactStore *contact_store;

		contact_store = e_name_selector_model_peek_contact_store (name_selector_dialog->priv->name_selector_model);
		if (contact_store) {
			g_signal_handlers_disconnect_by_func (contact_store, start_client_view_cb, name_selector_dialog);
			g_signal_handlers_disconnect_by_func (contact_store, stop_client_view_cb, name_selector_dialog);
			g_signal_handlers_disconnect_by_func (contact_store, start_update_cb, name_selector_dialog);
			g_signal_handlers_disconnect_by_func (contact_store, stop_update_cb, name_selector_dialog);
		}

		g_signal_handlers_disconnect_matched (
			name_selector_dialog->priv->name_selector_model,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, name_selector_dialog);

		g_object_unref (name_selector_dialog->priv->name_selector_model);
		name_selector_dialog->priv->name_selector_model = NULL;
	}
}

static void
contact_column_formatter (GtkTreeViewColumn *column,
                          GtkCellRenderer *cell,
                          GtkTreeModel *model,
                          GtkTreeIter *iter,
                          ENameSelectorDialog *name_selector_dialog)
{
	EContactStore *contact_store;
	EContact      *contact;
	GtkTreeIter    contact_store_iter;
	GList         *email_list;
	gchar         *string;
	gchar         *full_name_str;
	gchar         *email_str;
	gint           email_n;

	contact_store_iter = *iter;
	sort_iter_to_contact_store_iter (
		name_selector_dialog, &contact_store_iter, &email_n);

	contact_store = e_name_selector_model_peek_contact_store (
		name_selector_dialog->priv->name_selector_model);
	contact = e_contact_store_get_contact (
		contact_store, &contact_store_iter);
	email_list = e_name_selector_model_get_contact_emails_without_used (
		name_selector_dialog->priv->name_selector_model, contact, TRUE);
	email_str = g_list_nth_data (email_list, email_n);
	full_name_str = e_contact_get (contact, E_CONTACT_FULL_NAME);

	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
		if (!full_name_str)
			full_name_str = e_contact_get (contact, E_CONTACT_FILE_AS);
		string = g_strdup_printf ("%s", full_name_str ? full_name_str : "?");
	} else {
		string = g_strdup_printf (
			"%s%s<%s>", full_name_str ? full_name_str : "",
			full_name_str ? " " : "",
			email_str ? email_str : "");
	}

	g_free (full_name_str);
	e_name_selector_model_free_emails_list (email_list);

	g_object_set (cell, "text", string, NULL);
	g_free (string);
}

static void
destination_column_formatter (GtkTreeViewColumn *column,
                              GtkCellRenderer *cell,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              ENameSelectorDialog *name_selector_dialog)
{
	EDestinationStore *destination_store = E_DESTINATION_STORE (model);
	EDestination      *destination;
	GString           *buffer;

	destination = e_destination_store_get_destination (destination_store, iter);
	g_return_if_fail (destination);

	buffer = g_string_new (e_destination_get_name (destination));

	if (!e_destination_is_evolution_list (destination)) {
		const gchar *email;

		email = e_destination_get_email (destination);
		if (email == NULL || *email == '\0')
			email = "?";
		g_string_append_printf (buffer, " <%s>", email);
	}

	g_object_set (cell, "text", buffer->str, NULL);
	g_string_free (buffer, TRUE);
}

/* ----------------------- *
 * ENameSelectorDialog API *
 * ----------------------- */

/**
 * e_name_selector_dialog_peek_model:
 * @name_selector_dialog: an #ENameSelectorDialog
 *
 * Gets the #ENameSelectorModel used by @name_selector_model.
 *
 * Returns: The #ENameSelectorModel being used.
 **/
ENameSelectorModel *
e_name_selector_dialog_peek_model (ENameSelectorDialog *name_selector_dialog)
{
	g_return_val_if_fail (E_IS_NAME_SELECTOR_DIALOG (name_selector_dialog), NULL);

	return name_selector_dialog->priv->name_selector_model;
}

/**
 * e_name_selector_dialog_set_model:
 * @name_selector_dialog: an #ENameSelectorDialog
 * @model: an #ENameSelectorModel
 *
 * Sets the model being used by @name_selector_dialog to @model.
 **/
void
e_name_selector_dialog_set_model (ENameSelectorDialog *name_selector_dialog,
                                  ENameSelectorModel *model)
{
	g_return_if_fail (E_IS_NAME_SELECTOR_DIALOG (name_selector_dialog));
	g_return_if_fail (E_IS_NAME_SELECTOR_MODEL (model));

	if (model == name_selector_dialog->priv->name_selector_model)
		return;

	shutdown_name_selector_model (name_selector_dialog);
	name_selector_dialog->priv->name_selector_model = g_object_ref (model);

	setup_name_selector_model (name_selector_dialog);
}

/**
 * e_name_selector_dialog_set_destination_index:
 * @name_selector_dialog: an #ENameSelectorDialog
 * @index: index of the destination section, starting from 0.
 *
 * Sets the index number of the destination section.
 **/
void
e_name_selector_dialog_set_destination_index (ENameSelectorDialog *name_selector_dialog,
                                              guint index)
{
	g_return_if_fail (E_IS_NAME_SELECTOR_DIALOG (name_selector_dialog));

	if (index >= name_selector_dialog->priv->sections->len)
		return;

	name_selector_dialog->priv->destination_index = index;
}

/**
 * e_name_selector_dialog_set_scrolling_policy:
 * @name_selector_dialog: an #ENameSelectorDialog
 * @hscrollbar_policy: scrolling policy for horizontal bar of the contacts window.
 * @vscrollbar_policy: scrolling policy for vertical bar of the contacts window.
 *
 * Sets the scrolling policy for the contacts section.
 *
 * Since: 3.2
 **/
void
e_name_selector_dialog_set_scrolling_policy (ENameSelectorDialog *name_selector_dialog,
                                             GtkPolicyType hscrollbar_policy,
                                             GtkPolicyType vscrollbar_policy)
{
	GtkScrolledWindow *win = GTK_SCROLLED_WINDOW (name_selector_dialog->priv->contact_window);

	gtk_scrolled_window_set_policy (win, hscrollbar_policy, vscrollbar_policy);
}

/**
 * e_name_selector_dialog_get_section_visible:
 * @name_selector_dialog: an #ENameSelectorDialog
 * @name: name of the section
 *
 * Returns: whether section named @name is visible in the dialog.
 *
 * Since: 3.8
 **/
gboolean
e_name_selector_dialog_get_section_visible (ENameSelectorDialog *name_selector_dialog,
                                            const gchar *name)
{
	Section *section;
	gint index;

	g_return_val_if_fail (E_IS_NAME_SELECTOR_DIALOG (name_selector_dialog), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	index = find_section_by_name (name_selector_dialog, name);
	g_return_val_if_fail (index != -1, FALSE);

	section = &g_array_index (name_selector_dialog->priv->sections, Section, index);
	return gtk_widget_get_visible (GTK_WIDGET (section->section_grid));
}

/**
 * e_name_selector_dialog_set_section_visible:
 * @name_selector_dialog: an #ENameSelectorDialog
 * @name: name of the section
 * @visible: whether to show or hide the section
 *
 * Shows or hides section named @name in the dialog.
 *
 * Since: 3.8
 **/
void
e_name_selector_dialog_set_section_visible (ENameSelectorDialog *name_selector_dialog,
                                            const gchar *name,
                                            gboolean visible)
{
	Section *section;
	gint index;

	g_return_if_fail (E_IS_NAME_SELECTOR_DIALOG (name_selector_dialog));
	g_return_if_fail (name != NULL);

	index = find_section_by_name (name_selector_dialog, name);
	g_return_if_fail (index != -1);

	section = &g_array_index (name_selector_dialog->priv->sections, Section, index);

	if (visible)
		gtk_widget_show (GTK_WIDGET (section->section_grid));
	else
		gtk_widget_hide (GTK_WIDGET (section->section_grid));
}

