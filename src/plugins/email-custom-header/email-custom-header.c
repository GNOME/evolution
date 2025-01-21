/*
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
 *		Ashish Shrivastava <shashish@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "mail/em-utils.h"
#include "mail/em-event.h"
#include "composer/e-msg-composer.h"
#include "email-custom-header.h"

#define d(x)

#define ECM_SETTINGS_ID  "org.gnome.evolution.plugin.email-custom-header"
#define ECM_SETTINGS_KEY "custom-header"

typedef struct {
	GtkWidget *treeview;
	GtkWidget *header_add;
	GtkWidget *header_edit;
	GtkWidget *header_remove;
	GtkListStore *store;
} ConfigData;

enum {
	HEADER_KEY_COLUMN,
	HEADER_VALUE_COLUMN,
	HEADER_N_COLUMNS
};

struct _CustomHeaderOptionsDialogPrivate {
	GtkBuilder *builder;
	/*Widgets*/
	GtkWidget *main;
	GtkWidget *page;
	GtkWidget *header_table;
	GtkWidget *header_type_name_label;
	GArray *combo_box_header_value;
	GArray *email_custom_header_details;
	GArray *header_index_type;
	gint flag;
	gchar *help_section;
};

/* epech - e-plugin email custom header*/
GType custom_header_options_dialog_get_type (void);
static void epech_dialog_finalize (GObject *object);
static void epech_setup_widgets (CustomHeaderOptionsDialog *mch);
static void commit_changes (ConfigData *cd);
gint e_plugin_lib_enable (EPlugin *ep, gint enable);
GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *epl);
gboolean e_plugin_ui_init (EUIManager *manager, EMsgComposer *composer);
GtkWidget *org_gnome_email_custom_header_config_option (EPlugin *epl, struct _EConfigHookItemFactoryData *data);

G_DEFINE_TYPE_WITH_PRIVATE (CustomHeaderOptionsDialog, custom_header_options_dialog, G_TYPE_OBJECT)

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

static void
epech_get_widgets_data (CustomHeaderOptionsDialog *mch)
{
	CustomHeaderOptionsDialogPrivate *priv;
	HeaderValueComboBox *sub_combo_box_get;
	gint selected_item;
	gint index_column;

	priv = mch->priv;
	priv->header_index_type = g_array_new (FALSE, FALSE, sizeof (gint));
	priv->flag++;

	for (index_column = 0;
		index_column < priv->email_custom_header_details->len; index_column++) {

		sub_combo_box_get = &g_array_index (priv->combo_box_header_value, HeaderValueComboBox,index_column);
		selected_item = gtk_combo_box_get_active ((GtkComboBox *) sub_combo_box_get->header_value_combo_box);
		g_array_append_val (priv->header_index_type, selected_item);
	}
}

static gboolean
epech_get_widgets (CustomHeaderOptionsDialog *mch)
{
	CustomHeaderOptionsDialogPrivate *priv;
	priv = mch->priv;

#define EMAIL_CUSTOM_HEADER(name) e_builder_get_widget (priv->builder, name)
	priv->main = EMAIL_CUSTOM_HEADER ("email-custom-header-dialog");

	if (!priv->main)
		return FALSE;

	priv->page = EMAIL_CUSTOM_HEADER ("email-custom-header-vbox");
	priv->header_table = EMAIL_CUSTOM_HEADER ("email-custom-header-options");
#undef EMAIL_CUSTOM_HEADER

	return (priv->page
		&&priv->header_table);
}

static void
epech_fill_widgets_with_data (CustomHeaderOptionsDialog *mch)
{
	CustomHeaderOptionsDialogPrivate *priv;
	HeaderValueComboBox *sub_combo_box_fill;
	gint set_index_column;

	priv = mch->priv;
	priv->help_section = g_strdup ("mail-composer-custom-header-lines");

	for (set_index_column = 0;
		set_index_column < priv->email_custom_header_details->len; set_index_column++) {
		sub_combo_box_fill = &g_array_index (priv->combo_box_header_value, HeaderValueComboBox,set_index_column);

		if (priv->flag == 0) {
			gtk_combo_box_set_active ((GtkComboBox *) sub_combo_box_fill->header_value_combo_box,0);
		} else {
			gtk_combo_box_set_active (
				(GtkComboBox *) sub_combo_box_fill->header_value_combo_box,
				g_array_index (priv->header_index_type, gint, set_index_column));
		}
	}
}

CustomHeaderOptionsDialog *
epech_dialog_new (void)
{
	return g_object_new (E_TYPE_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG, NULL);
}

static void
epech_header_options_cb (GtkDialog *dialog,
                         gint state,
                         gpointer func_data)
{
	CustomHeaderOptionsDialogPrivate *priv;
	CustomHeaderOptionsDialog *mch;

	mch = func_data;
	priv = mch->priv;

	switch (state) {
		case GTK_RESPONSE_OK:
			epech_get_widgets_data (mch);
			/* coverity[fallthrough] */
			/* falls through */
		case GTK_RESPONSE_CANCEL:
			gtk_widget_hide (priv->main);
			gtk_widget_destroy (priv->main);
			g_object_unref (priv->builder);
			break;
		case GTK_RESPONSE_HELP:
			e_display_help (
				GTK_WINDOW (priv->main),
				priv->help_section);
			break;
	}

	g_signal_emit (func_data, signals[MCH_RESPONSE], 0, state);
}

static gboolean
epech_dialog_run (CustomHeaderOptionsDialog *mch,
                  GtkWidget *parent)
{
	CustomHeaderOptionsDialogPrivate *priv;
	GSettings *settings;
	GtkWidget *toplevel;

	g_return_val_if_fail (mch != NULL || E_IS_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG (mch), FALSE);
	priv = mch->priv;

	settings = e_util_ref_settings (ECM_SETTINGS_ID);
	epech_load_from_settings (settings, ECM_SETTINGS_KEY, mch);
	g_object_unref (settings);

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (
		priv->builder, "org-gnome-email-custom-header.ui");

	if (!epech_get_widgets (mch)) {
		g_object_unref (priv->builder);
		d (printf ("\n Could not get the Widgets\n"));
	}

	epech_setup_widgets (mch);
	toplevel = gtk_widget_get_toplevel (priv->main);

	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (toplevel),GTK_WINDOW (parent));

	epech_fill_widgets_with_data (mch);
	g_signal_connect (
		priv->main, "response",
		G_CALLBACK (epech_header_options_cb), mch);
	gtk_widget_show (priv->main);

	return TRUE;
}

static void
epech_load_from_settings (GSettings *settings,
                          const gchar *key,
                          CustomHeaderOptionsDialog *mch)
{
	CustomHeaderOptionsDialogPrivate *priv;
	EmailCustomHeaderDetails temp_header_details= {-1, -1, NULL, NULL};
	CustomSubHeader temp_header_value_details = {NULL};
	gchar **headers;
	gint index,pos;

	priv = mch->priv;
	priv->email_custom_header_details = g_array_new (TRUE, TRUE, sizeof (EmailCustomHeaderDetails));
	headers = g_settings_get_strv (settings, key);

	for (pos = 0; headers && headers[pos]; pos++) {
		gchar **parse_header_list;

		memset (&temp_header_value_details, 0, sizeof (CustomSubHeader));
		temp_header_details.sub_header_type_value = g_array_new (TRUE, TRUE, sizeof (CustomSubHeader));
		parse_header_list = g_strsplit_set (headers[pos], "=;,", -1);
		temp_header_details.header_type_value = g_string_new ("");
		if (temp_header_details.header_type_value) {
			g_string_assign (temp_header_details.header_type_value, parse_header_list[0]);
		}

		for (index = 0; parse_header_list[index + 1] ; ++index) {
			temp_header_value_details.sub_header_string_value = g_string_new ("");

			if (temp_header_value_details.sub_header_string_value) {
				g_string_assign (temp_header_value_details.sub_header_string_value, parse_header_list[index + 1]);
			}

			g_array_append_val (temp_header_details.sub_header_type_value, temp_header_value_details);
		}

		temp_header_details.number_of_subtype_header = index;
		g_array_append_val (priv->email_custom_header_details, temp_header_details);

		g_strfreev (parse_header_list);
	}

	temp_header_details.number_of_header = pos;

	g_strfreev (headers);
}

static void
epech_setup_widgets (CustomHeaderOptionsDialog *mch)
{
	CustomHeaderOptionsDialogPrivate *priv;
	EmailCustomHeaderDetails *temp_header_ptr,*temp;
	CustomSubHeader *temp_header_value_ptr;
	HeaderValueComboBox sub_combo_box = {NULL};
	HeaderValueComboBox *sub_combo_box_ptr;
	gint sub_index,row_combo;
	gint header_section_id,sub_type_index,row;
	gint i;
	const gchar *str;
	static const gchar *security_field = NC_("email-custom-header-Security", "Security:");
	static struct _security_values {
		const gchar *value, *str;
	} security_values[] = {
		{ "Personal",     NC_("email-custom-header-Security", "Personal") } ,
		{ "Unclassified", NC_("email-custom-header-Security", "Unclassified") },
		{ "Protected",    NC_("email-custom-header-Security", "Protected") },
		{ "InConfidence", NC_("email-custom-header-Security", "Confidential") },
		{ "Secret",       NC_("email-custom-header-Security", "Secret") },
		{ "Topsecret",    NC_("email-custom-header-Security", "Top secret") },
		{ NULL, NULL }
	};

	priv = mch->priv;
	priv->combo_box_header_value = g_array_new (TRUE, FALSE, sizeof (HeaderValueComboBox));

	for (header_section_id = 0,row = 0;
		header_section_id < priv->email_custom_header_details->len; header_section_id++,row++) {

		/* To create an empty label widget. Text will be added dynamically. */
		priv->header_type_name_label = gtk_label_new (NULL);
		temp_header_ptr = &g_array_index (priv->email_custom_header_details, EmailCustomHeaderDetails,header_section_id);
		str = (temp_header_ptr->header_type_value)->str;
		if (strcmp (str, security_field) == 0)
			str = g_dpgettext2 (GETTEXT_PACKAGE, "email-custom-header-Security", security_field);
		gtk_label_set_markup (GTK_LABEL (priv->header_type_name_label), str);

		gtk_grid_attach (
			GTK_GRID (priv->header_table),
			priv->header_type_name_label, 0, row, 1, 1);

		gtk_label_set_xalign (GTK_LABEL (priv->header_type_name_label), 1);
		gtk_widget_show (priv->header_type_name_label);
		sub_combo_box.header_value_combo_box = gtk_combo_box_text_new ();
		g_array_append_val (priv->combo_box_header_value, sub_combo_box);
	}

	for (sub_index = 0,row_combo = 0; sub_index < priv->combo_box_header_value->len;
		sub_index++,row_combo++) {
		temp = &g_array_index (priv->email_custom_header_details, EmailCustomHeaderDetails,sub_index);

		sub_combo_box_ptr = &g_array_index (priv->combo_box_header_value, HeaderValueComboBox,sub_index);
		gtk_grid_attach (
			GTK_GRID (priv->header_table),
			sub_combo_box_ptr->header_value_combo_box, 1, row_combo, 1, 1);

		for (sub_type_index = 0; sub_type_index < temp->number_of_subtype_header; sub_type_index++) {
			temp_header_value_ptr = &g_array_index (temp->sub_header_type_value, CustomSubHeader,sub_type_index);
			str = (temp_header_value_ptr->sub_header_string_value)->str;
			for (i = 0; security_values[i].value != NULL; i++) {
				if (strcmp (str, security_values[i].value) == 0) {
					str = g_dpgettext2 (GETTEXT_PACKAGE, "email-custom-header-Security", security_values[i].str);
					break;
				}
			}
			gtk_combo_box_text_append_text (
				GTK_COMBO_BOX_TEXT (
				sub_combo_box_ptr->header_value_combo_box), str);
		}

		gtk_combo_box_text_append_text (
			GTK_COMBO_BOX_TEXT (
			sub_combo_box_ptr->header_value_combo_box),
			/* Translators: "None" as an email custom header option in a dialog invoked by Insert->Custom Header from Composer,
			 * indicating the header will not be added to a mail message */
			C_("email-custom-header", "None"));
		gtk_widget_set_hexpand (sub_combo_box_ptr->header_value_combo_box, TRUE);
		gtk_widget_show (sub_combo_box_ptr->header_value_combo_box);
	}
}

static void
custom_header_options_dialog_class_init (CustomHeaderOptionsDialogClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = epech_dialog_finalize;

	signals[MCH_RESPONSE] = g_signal_new (
		"emch_response",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (CustomHeaderOptionsDialogClass, emch_response),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);
}

static void
custom_header_options_dialog_init (CustomHeaderOptionsDialog *mch)
{
	mch->priv = custom_header_options_dialog_get_instance_private (mch);
}

static void
epech_dialog_finalize (GObject *object)
{
	CustomHeaderOptionsDialog *self = E_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG (object);

	g_free (self->priv->help_section);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (custom_header_options_dialog_parent_class)->finalize (object);
}

static void
epech_append_to_custom_header (CustomHeaderOptionsDialog *dialog,
                               gint state,
                               gpointer data)
{
	EMsgComposer *composer;
	CustomHeaderOptionsDialogPrivate *priv;
	EmailCustomHeaderDetails *temp_header_ptr;
	CustomSubHeader *temp_header_value_ptr;
	gint index_subtype,sub_type_index;

	composer = (EMsgComposer *) data;
	priv = dialog->priv;

	if (state == GTK_RESPONSE_OK) {

		for (index_subtype = 0; index_subtype < priv->email_custom_header_details->len; index_subtype++) {

			temp_header_ptr = &g_array_index (priv->email_custom_header_details, EmailCustomHeaderDetails,index_subtype);

			for (sub_type_index = 0; sub_type_index < temp_header_ptr->number_of_subtype_header; sub_type_index++) {
				temp_header_value_ptr = &g_array_index (temp_header_ptr->sub_header_type_value, CustomSubHeader,sub_type_index);

				if (sub_type_index == g_array_index (priv->header_index_type, gint, index_subtype)) {
					e_msg_composer_set_header (
						composer, (temp_header_ptr->header_type_value)->str,
						(temp_header_value_ptr->sub_header_string_value)->str);
				}
			}
		}
	}
}

static void
epech_custom_header_options_commit (EMsgComposer *composer,
                                    gpointer user_data)
{
	g_object_set_data ((GObject *) composer, "epech_dialog", NULL);
}

static void
action_email_custom_header_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMsgComposer *composer = user_data;
	CustomHeaderOptionsDialog *dialog = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	dialog = g_object_get_data ((GObject *) composer, "epech_dialog");

	if (!dialog) {
		dialog = epech_dialog_new ();
		if (dialog) {
			g_object_set_data ((GObject *) composer, "epech_dialog", dialog);

			g_signal_connect (
				dialog, "emch_response",
				G_CALLBACK (epech_append_to_custom_header), composer);
			g_signal_connect (
				composer, "destroy",
				G_CALLBACK (epech_custom_header_options_commit), composer);
		}
	}

	epech_dialog_run (dialog, GTK_WIDGET (composer));
}

gboolean
e_plugin_ui_init (EUIManager *manager,
                  EMsgComposer *composer)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='insert-menu'>"
		      "<placeholder id='insert-menu-top'>"
			"<item action='custom-header'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";
	static const EUIActionEntry entries[] = {

		{ "custom-header",
		  NULL,
		  N_("_Custom Header"),
		  NULL,
		  NULL,
		  action_email_custom_header_cb, NULL, NULL, NULL }
	};

	EHTMLEditor *editor;
	EUIManager *ui_manager;

	editor = e_msg_composer_get_editor (composer);
	ui_manager = e_html_editor_get_ui_manager (editor);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "composer", GETTEXT_PACKAGE,
		entries, G_N_ELEMENTS (entries), composer, eui);

	return TRUE;
}

static void
commit_changes (ConfigData *cd)
{
	GtkTreeModel *model = NULL;
	GPtrArray *headers;
	GtkTreeIter iter;
	gboolean valid;
	GSettings *settings;

	headers = g_ptr_array_new_full (3, g_free);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (cd->treeview));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gchar *keyword = NULL, *value = NULL;

		gtk_tree_model_get (
			model, &iter,
			HEADER_KEY_COLUMN, &keyword,
			HEADER_VALUE_COLUMN, &value,
			-1);

                /* Check if the keyword is not empty */
		if ((keyword) && (g_utf8_strlen (g_strstrip (keyword), -1) > 0)) {
			if ((value) && (g_utf8_strlen (g_strstrip (value), -1) > 0)) {
				gchar *tmp = keyword;

				keyword = g_strconcat (keyword, "=", value, NULL);
				g_free (tmp);
			}
			g_ptr_array_add (headers, g_strdup (keyword));
		}

		g_free (keyword);
		g_free (value);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	g_ptr_array_add (headers, NULL);

	settings = e_util_ref_settings (ECM_SETTINGS_ID);
	g_settings_set_strv (settings, ECM_SETTINGS_KEY, (const gchar * const *) headers->pdata);
	g_object_unref (settings);

	g_ptr_array_free (headers, TRUE);
}

static void
cell_edited_cb (GtkCellRendererText *cell,
                gchar *path_string,
                gchar *new_text,
                ConfigData *cd)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (cd->treeview));
	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	if (new_text == NULL || *g_strstrip (new_text) == '\0')
		gtk_button_clicked (GTK_BUTTON (cd->header_remove));
	else {
		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			HEADER_KEY_COLUMN, new_text, -1);
		commit_changes (cd);
	}
}

static void
cell_editing_canceled_cb (GtkCellRenderer *cell,
                          ConfigData *cd)
{
	gtk_button_clicked (GTK_BUTTON (cd->header_remove));
}

static void
cell_value_edited_cb (GtkCellRendererText *cell,
                      gchar *path_string,
                      gchar *new_text,
                      ConfigData *cd)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (cd->treeview));

	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		HEADER_VALUE_COLUMN, new_text, -1);

	commit_changes (cd);
}

static void
header_add_clicked (GtkButton *button,
                    ConfigData *cd)
{
	GtkTreeModel *model;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GtkTreeIter iter;

	tree_view = GTK_TREE_VIEW (cd->treeview);
	model = gtk_tree_view_get_model (tree_view);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	path = gtk_tree_model_get_path (model, &iter);
	column = gtk_tree_view_get_column (tree_view, HEADER_KEY_COLUMN);
	gtk_tree_view_set_cursor (tree_view, path, column, TRUE);
	gtk_tree_view_row_activated (tree_view, path, column);
	gtk_tree_path_free (path);
}

static void
header_remove_clicked (GtkButton *button,
                       ConfigData *cd)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid;
	gint len;

	valid = FALSE;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cd->treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	/* Get the path and move to the previous node :) */
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
		gtk_widget_set_sensitive (cd->header_edit, FALSE);
		gtk_widget_set_sensitive (cd->header_remove, FALSE);
	}

	gtk_widget_grab_focus (cd->treeview);
	gtk_tree_path_free (path);

	commit_changes (cd);
}

static void
header_edit_clicked (GtkButton *button,
                     ConfigData *cd)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeViewColumn *focus_col;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cd->treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	focus_col = gtk_tree_view_get_column (GTK_TREE_VIEW (cd->treeview), HEADER_KEY_COLUMN);
	path = gtk_tree_model_get_path (model, &iter);

	if (path) {
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (cd->treeview), path, focus_col, TRUE);
		gtk_tree_path_free (path);
	}
}

static void
selection_changed (GtkTreeSelection *selection,
                   ConfigData *cd)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_widget_set_sensitive (cd->header_edit, TRUE);
		gtk_widget_set_sensitive (cd->header_remove, TRUE);
	} else {
		gtk_widget_set_sensitive (cd->header_edit, FALSE);
		gtk_widget_set_sensitive (cd->header_remove, FALSE);
	}
}

static void
destroy_cd_data (gpointer data)
{
	ConfigData *cd = (ConfigData *) data;

	g_free (cd);
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkWidget *hbox;
	gchar **headers;
	gint index;
	GtkTreeViewColumn *col;
	gint col_pos;
	GSettings *settings;
	ConfigData *cd = g_new0 (ConfigData, 1);

	GtkWidget *ech_configuration_box;
	GtkWidget *vbox2;
	GtkWidget *label1;
	GtkWidget *header_configuration_box;
	GtkWidget *header_container;
	GtkWidget *scrolledwindow1;
	GtkWidget *header_treeview;
	GtkWidget *vbuttonbox1;
	GtkWidget *header_add;
	GtkWidget *header_edit;
	GtkWidget *header_remove;

	ech_configuration_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (ech_configuration_box);

	vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (ech_configuration_box), vbox2, FALSE, FALSE, 0);

	/* To translators: This string is used while adding a new message header to configuration, to specifying the format of the key values */
	label1 = gtk_label_new (_("The format for specifying a Custom Header key value is:\nName of the Custom Header key values separated by “;”."));
	gtk_widget_show (label1);
	gtk_box_pack_start (GTK_BOX (vbox2), label1, FALSE, TRUE, 0);
	gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap (GTK_LABEL (label1), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (label1), 20);

	header_configuration_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (header_configuration_box);
	gtk_box_pack_start (GTK_BOX (ech_configuration_box), header_configuration_box, TRUE, TRUE, 0);

	header_container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_show (header_container);
	gtk_box_pack_start (GTK_BOX (header_configuration_box), header_container, TRUE, TRUE, 0);

	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow1);
	gtk_box_pack_start (GTK_BOX (header_container), scrolledwindow1, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	header_treeview = gtk_tree_view_new ();
	gtk_widget_show (header_treeview);
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), header_treeview);
	gtk_container_set_border_width (GTK_CONTAINER (header_treeview), 1);

	vbuttonbox1 = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (vbuttonbox1);
	gtk_box_pack_start (GTK_BOX (header_container), vbuttonbox1, FALSE, TRUE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox1), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox1), 6);

	header_add = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	gtk_widget_show (header_add);
	gtk_container_add (GTK_CONTAINER (vbuttonbox1), header_add);
	gtk_widget_set_can_default (header_add, TRUE);

	header_edit = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_widget_show (header_edit);
	gtk_container_add (GTK_CONTAINER (vbuttonbox1), header_edit);
	gtk_widget_set_can_default (header_edit, TRUE);

	header_remove = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	gtk_widget_show (header_remove);
	gtk_container_add (GTK_CONTAINER (vbuttonbox1), header_remove);
	gtk_widget_set_can_default (header_remove, TRUE);

	cd->treeview = header_treeview;

	cd->store = gtk_list_store_new (HEADER_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (cd->treeview), GTK_TREE_MODEL (cd->store));

	renderer = gtk_cell_renderer_text_new ();
	col_pos = gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (cd->treeview), -1, _("Key"),
		renderer, "text", HEADER_KEY_COLUMN, NULL);
	col = gtk_tree_view_get_column (GTK_TREE_VIEW (cd->treeview), col_pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable (col, TRUE);
	g_object_set (col, "min-width", 50, NULL);

	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (cell_edited_cb), cd);
	g_signal_connect (
		renderer, "editing-canceled",
		G_CALLBACK (cell_editing_canceled_cb), cd);

	renderer = gtk_cell_renderer_text_new ();
	col_pos = gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (cd->treeview), -1, _("Values"),
			renderer, "text", HEADER_VALUE_COLUMN, NULL);
	col = gtk_tree_view_get_column (GTK_TREE_VIEW (cd->treeview), col_pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable (col, TRUE);
	g_object_set (renderer, "editable", TRUE, NULL);

	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (cell_value_edited_cb), cd);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cd->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (selection_changed), cd);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (cd->treeview), TRUE);

	cd->header_add = header_add;
	g_signal_connect (
		cd->header_add, "clicked",
		G_CALLBACK (header_add_clicked), cd);

	cd->header_remove = header_remove;
	g_signal_connect (
		cd->header_remove, "clicked",
		G_CALLBACK (header_remove_clicked), cd);
	gtk_widget_set_sensitive (cd->header_remove, FALSE);

	cd->header_edit = header_edit;
	g_signal_connect (
		cd->header_edit, "clicked",
		G_CALLBACK (header_edit_clicked), cd);
	gtk_widget_set_sensitive (cd->header_edit, FALSE);

	/* Populate tree view with values from settings */
	settings = e_util_ref_settings (ECM_SETTINGS_ID);
	headers = g_settings_get_strv (settings, ECM_SETTINGS_KEY);
	g_object_unref (settings);

	if (headers) {
		gint ii;

		for (ii = 0; headers[ii]; ii++) {
			gchar **parse_header_list;

			gtk_list_store_append (cd->store, &iter);

			parse_header_list = g_strsplit_set (headers[ii], "=,", -1);

			gtk_list_store_set (cd->store, &iter, HEADER_KEY_COLUMN, parse_header_list[0], -1);

			for (index = 0; parse_header_list[index + 1] ; ++index) {
				gtk_list_store_set (cd->store, &iter, HEADER_VALUE_COLUMN, parse_header_list[index + 1], -1);
			}

			g_strfreev (parse_header_list);
		}

		g_strfreev (headers);
	}

	/* Add the list here */

	hbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	gtk_box_pack_start (GTK_BOX (hbox), ech_configuration_box, TRUE, TRUE, 0);

	/* to let free data properly on destroy of configuration widget */
	g_object_set_data_full (G_OBJECT (hbox), "mycd-data", cd, destroy_cd_data);

	return hbox;
}

/* Configuration in Mail Prefs Page goes here */

GtkWidget *
org_gnome_email_custom_header_config_option (EPlugin *epl,
                                             struct _EConfigHookItemFactoryData *data)
{
	/* This function and the hook needs to be removed,
	once the configure code is thoroughly tested */

	return NULL;

}
