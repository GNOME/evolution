/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Ashish Shrivastava <shashish@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include "mail/em-utils.h"
#include "mail/em-event.h"
#include "composer/e-msg-composer.h"
#include "libedataserver/e-account.h"
#include "e-util/e-config.h"
#include "e-util/e-util.h"
#include "email-custom-header.h"

#define d(x)
#define GCONF_KEY_CUSTOM_HEADER "/apps/evolution/eplugin/email_custom_header/customHeader"

typedef struct {
        GConfClient *gconf;
        GtkWidget   *treeview;
        GtkWidget   *header_add;
        GtkWidget   *header_edit;
        GtkWidget   *header_remove;
        GtkListStore *store;
} ConfigData;

enum {
        HEADER_KEY_COLUMN,
	HEADER_VALUE_COLUMN,
        HEADER_N_COLUMNS
};

struct _EmailCustomHeaderOptionsDialogPrivate {
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
static void epech_dialog_class_init (GObjectClass *object_class);
static void epech_dialog_finalize (GObject *object);
static void epech_dialog_init (GObject *object);
static void epech_dialog_dispose (GObject *object);
static void epech_setup_widgets (CustomHeaderOptionsDialog *mch);
static gint epech_check_existing_composer_window(gconstpointer a, gconstpointer b);
static void commit_changes (ConfigData *cd);
gint e_plugin_lib_enable (EPlugin *ep, gint enable);
GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *epl);
gboolean e_plugin_ui_init(GtkUIManager *ui_manager, EMsgComposer *composer);
GtkWidget *org_gnome_email_custom_header_config_option (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
        return 0;
}

static void
epech_get_widgets_data (CustomHeaderOptionsDialog *mch)
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	HeaderValueComboBox *sub_combo_box_get;
	gint selected_item;
	gint index_column;

	priv = mch->priv;
	priv->header_index_type = g_array_new (FALSE, FALSE, sizeof (gint));
	priv->flag++;

	for (index_column = 0;
		index_column < priv->email_custom_header_details->len; index_column++) {

		sub_combo_box_get = &g_array_index(priv->combo_box_header_value, HeaderValueComboBox,index_column);
		selected_item = gtk_combo_box_get_active((GtkComboBox *)sub_combo_box_get->header_value_combo_box);
		g_array_append_val (priv->header_index_type, selected_item);
	}
}

static gboolean
epech_get_widgets (CustomHeaderOptionsDialog *mch)
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	priv = mch->priv;

#define EMAIL_CUSTOM_HEADER(name) e_builder_get_widget (priv->builder, name)
	priv->main = EMAIL_CUSTOM_HEADER ("email-custom-header-dialog");

	if (!priv->main)
		return FALSE;

	priv->page  = EMAIL_CUSTOM_HEADER ("email-custom-header-vbox");
	priv->header_table = EMAIL_CUSTOM_HEADER ("email-custom-header-options");
#undef EMAIL_CUSTOM_HEADER

	return (priv->page
		&&priv->header_table);
}

static void
epech_fill_widgets_with_data (CustomHeaderOptionsDialog *mch)
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	HeaderValueComboBox *sub_combo_box_fill;
	gint set_index_column;

	priv = mch->priv;
	priv->help_section = g_strdup ("usage-mail");

	for (set_index_column = 0;
		set_index_column < priv->email_custom_header_details->len;set_index_column++) {
		sub_combo_box_fill = &g_array_index(priv->combo_box_header_value, HeaderValueComboBox,set_index_column);

		if (priv->flag == 0) {
			gtk_combo_box_set_active ((GtkComboBox *)sub_combo_box_fill->header_value_combo_box,0);
		} else {
			gtk_combo_box_set_active ((GtkComboBox *)sub_combo_box_fill->header_value_combo_box,
				g_array_index(priv->header_index_type, gint, set_index_column));
		}
	}
}

CustomHeaderOptionsDialog *
epech_dialog_new (void)
{
	CustomHeaderOptionsDialog *mch;

	mch = g_object_new (EMAIL_CUSTOM_HEADER_OPTIONS_DIALOG, NULL);

	return mch;
}

GType
epech_dialog_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (CustomHeaderOptionsDialogClass),
			NULL,   /* base_init */
			NULL,   /* base_finalize */
			(GClassInitFunc) epech_dialog_class_init,   /* class_init */
			NULL,   /* class_finalize */
			NULL,   /* class_data */
			sizeof (CustomHeaderOptionsDialog),
			0,      /* n_preallocs */
			(GInstanceInitFunc) epech_dialog_init,
			NULL    /* instance_init */
		};
		type = g_type_register_static (G_TYPE_OBJECT,
				"CustomHeaderOptionsDialogType",
				&info, 0);
	}

	return type;
}

static void
epech_header_options_cb (GtkDialog *dialog, gint state, gpointer func_data)
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	CustomHeaderOptionsDialog *mch;

	mch = func_data;
	priv = mch->priv;

	switch (state) {
		case GTK_RESPONSE_OK:
			epech_get_widgets_data (mch);
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

	g_signal_emit (G_OBJECT (func_data), signals[MCH_RESPONSE], 0, state);
}

static gboolean
epech_dialog_run (CustomHeaderOptionsDialog *mch, GtkWidget *parent)
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	GtkWidget *toplevel;

	g_return_val_if_fail (mch != NULL || EMAIL_CUSTOM_HEADER_OPTIONS_IS_DIALOG (mch), FALSE);
	priv = mch->priv;
	epech_get_header_list (mch);

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (
		priv->builder, "org-gnome-email-custom-header.ui");

	if (!epech_get_widgets(mch)) {
		g_object_unref (priv->builder);
		d (printf ("\n Could not get the Widgets\n"));
	}

	epech_setup_widgets (mch);
	toplevel =  gtk_widget_get_toplevel (priv->main);

	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (toplevel),GTK_WINDOW (parent));

	epech_fill_widgets_with_data (mch);
	g_signal_connect (GTK_DIALOG (priv->main), "response", G_CALLBACK(epech_header_options_cb), mch);
	gtk_widget_show (priv->main);

	return TRUE;
}

static void
epech_get_header_list (CustomHeaderOptionsDialog *mch)
{
	GConfClient *client;

	client = gconf_client_get_default ();
	g_return_if_fail (GCONF_IS_CLIENT (client));
	gconf_client_add_dir (client, GCONF_KEY_CUSTOM_HEADER, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	epech_load_from_gconf (client, "/apps/evolution/eplugin/email_custom_header/customHeader", mch);

	return;
}

static void
epech_load_from_gconf (GConfClient *client,const gchar *path,CustomHeaderOptionsDialog *mch)
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	EmailCustomHeaderDetails temp_header_details= {-1, -1, NULL, NULL};
	CustomSubHeader temp_header_value_details =  {NULL};
	GSList *header_list,*q;
	gchar *buffer;
	gint index,pos;

	priv = mch->priv;
	priv->email_custom_header_details = g_array_new (TRUE, TRUE, sizeof (EmailCustomHeaderDetails));
	header_list = gconf_client_get_list (client,path,GCONF_VALUE_STRING, NULL);

	for (q = header_list,pos = 0; q != NULL; q = q->next,pos++) {
		gchar **parse_header_list;

		memset(&temp_header_value_details,0,sizeof(CustomSubHeader));
		temp_header_details.sub_header_type_value = g_array_new (TRUE, TRUE, sizeof (CustomSubHeader));
		buffer = q->data;
		parse_header_list = g_strsplit_set (buffer, "=;,", -1);
		temp_header_details.header_type_value = g_string_new("");
		if (temp_header_details.header_type_value) {
			g_string_assign(temp_header_details.header_type_value, parse_header_list[0]);
		}

		for (index = 0; parse_header_list[index+1] ; ++index) {
			temp_header_value_details.sub_header_string_value = g_string_new("");

			if (temp_header_value_details.sub_header_string_value) {
				g_string_assign(temp_header_value_details.sub_header_string_value, parse_header_list[index+1]);
			}

			g_array_append_val(temp_header_details.sub_header_type_value, temp_header_value_details);
		}

		temp_header_details.number_of_subtype_header = index;
		g_array_append_val(priv->email_custom_header_details, temp_header_details);
	}

	temp_header_details.number_of_header = pos;
}

static void
epech_setup_widgets (CustomHeaderOptionsDialog *mch)
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	EmailCustomHeaderDetails *temp_header_ptr,*temp;
	CustomSubHeader *temp_header_value_ptr;
	HeaderValueComboBox sub_combo_box = {NULL};
	HeaderValueComboBox *sub_combo_box_ptr;
	gint sub_index,row_combo,column_combo;
	gint header_section_id,sub_type_index,row,column;
	gint i;
	gchar *str;
	static const gchar *security_field = N_("Security:");
	static struct _security_values {
		const gchar *value, *str;
	} security_values[] = {
		{ "Personal", N_("Personal") } ,
		{ "Unclassified", N_("Unclassified") },
		{ "Protected", N_("Protected") },
		{ "InConfidence", N_("Confidential") },
		{ "Secret", N_("Secret") },
		{ "Topsecret", N_("Top secret") },
		{ NULL, NULL }
	};

	priv = mch->priv;
	priv->combo_box_header_value = g_array_new (TRUE, FALSE, sizeof (HeaderValueComboBox));

	for (header_section_id = 0,row = 0,column = 1;
		header_section_id < priv->email_custom_header_details->len; header_section_id++,row++,column++) {

		/* To create an empty label widget. Text will be added dynamically. */
		priv->header_type_name_label = gtk_label_new ("");
		temp_header_ptr = &g_array_index(priv->email_custom_header_details, EmailCustomHeaderDetails,header_section_id);
                str = (temp_header_ptr->header_type_value)->str;
                if (strcmp (str, security_field) == 0) {
			str = _(security_field);
                }
		gtk_label_set_markup (GTK_LABEL (priv->header_type_name_label), str);

		gtk_table_attach (GTK_TABLE (priv->header_table), priv->header_type_name_label, 0, 1, row, column,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);

		gtk_misc_set_alignment (GTK_MISC (priv->header_type_name_label), 0, 0.5);
		gtk_widget_show (priv->header_type_name_label);
		sub_combo_box.header_value_combo_box = gtk_combo_box_new_text ();
		g_array_append_val(priv->combo_box_header_value, sub_combo_box);
	}

	for (sub_index = 0,row_combo = 0,column_combo = 1; sub_index < priv->combo_box_header_value->len;
                sub_index++,row_combo++,column_combo++) {
		temp = &g_array_index(priv->email_custom_header_details, EmailCustomHeaderDetails,sub_index);

		sub_combo_box_ptr = &g_array_index(priv->combo_box_header_value, HeaderValueComboBox,sub_index);
		gtk_table_attach (GTK_TABLE (priv->header_table),
			sub_combo_box_ptr->header_value_combo_box, 1, 2, row_combo, column_combo,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (GTK_FILL), 0, 0);

		for (sub_type_index = 0; sub_type_index < temp->number_of_subtype_header; sub_type_index++) {
			temp_header_value_ptr = &g_array_index(temp->sub_header_type_value, CustomSubHeader,sub_type_index);
			str = (temp_header_value_ptr->sub_header_string_value)->str;
			for (i = 0; security_values[i].value != NULL; i++) {
				if (strcmp (str, security_values[i].value) == 0) {
					str = _(security_values[i].str);
					break;
				}
			}
			gtk_combo_box_append_text (GTK_COMBO_BOX (sub_combo_box_ptr->header_value_combo_box),
				str);
		}

		/* Translators: "None" as an email custom header option in a dialog invoked by Insert->Custom Header from Composer,
		   indicating the header will not be added to a mail message */
		gtk_combo_box_append_text (GTK_COMBO_BOX (sub_combo_box_ptr->header_value_combo_box), C_("email-custom-header", "None"));
		gtk_widget_show (sub_combo_box_ptr->header_value_combo_box);
	}
}

static void
epech_dialog_class_init (GObjectClass *object)
{
	CustomHeaderOptionsDialogClass *klass;
	GObjectClass *object_class;

	klass = EMAIL_CUSTOM_HEADEROPTIONS_DIALOG_CLASS (object);
	parent_class = g_type_class_peek_parent (klass);
	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = epech_dialog_finalize;
	object_class->dispose = epech_dialog_dispose;

	signals[MCH_RESPONSE] = g_signal_new ("emch_response",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (CustomHeaderOptionsDialogClass, emch_response),
			NULL, NULL,
			g_cclosure_marshal_VOID__INT,
			G_TYPE_NONE, 1,
			G_TYPE_INT);
}

static void
epech_dialog_init (GObject *object)
{
	CustomHeaderOptionsDialog *mch;
	EmailCustomHeaderOptionsDialogPrivate *priv;

	mch = EMAIL_CUSTOM_HEADEROPTIONS_DIALOG (object);
	priv = g_new0 (EmailCustomHeaderOptionsDialogPrivate, 1);
	mch->priv = priv;
	priv->builder = NULL;
	priv->main = NULL;
	priv->page = NULL;
	priv->header_table = NULL;
}

static void
epech_dialog_finalize (GObject *object)
{
	CustomHeaderOptionsDialog *mch = (CustomHeaderOptionsDialog *)object;
	EmailCustomHeaderOptionsDialogPrivate *priv;

	g_return_if_fail (EMAIL_CUSTOM_HEADER_OPTIONS_IS_DIALOG (mch));
	priv = mch->priv;
	g_free (priv->help_section);

	if (mch->priv) {
		g_free (mch->priv);
		mch->priv = NULL;
	}

	if (parent_class->finalize)
		(* parent_class->finalize) (object);
}

static void
epech_dialog_dispose (GObject *object)
{
	CustomHeaderOptionsDialog *mch = (CustomHeaderOptionsDialog *) object;

	g_return_if_fail (EMAIL_CUSTOM_HEADER_OPTIONS_IS_DIALOG (mch));

	if (parent_class->dispose)
		(* parent_class->dispose) (object);
}

static void
epech_append_to_custom_header (CustomHeaderOptionsDialog *dialog, gint state, gpointer data)
{
	EMsgComposer *composer;
	EmailCustomHeaderOptionsDialogPrivate *priv;
	EmailCustomHeaderDetails *temp_header_ptr;
	CustomSubHeader *temp_header_value_ptr;
	gint index_subtype,sub_type_index;

	composer = (EMsgComposer *)data;
	priv = dialog->priv;

	if (state == GTK_RESPONSE_OK) {

		for (index_subtype = 0; index_subtype < priv->email_custom_header_details->len; index_subtype++) {

			temp_header_ptr = &g_array_index(priv->email_custom_header_details, EmailCustomHeaderDetails,index_subtype);

			for (sub_type_index = 0; sub_type_index < temp_header_ptr->number_of_subtype_header; sub_type_index++) {
				temp_header_value_ptr = &g_array_index(temp_header_ptr->sub_header_type_value, CustomSubHeader,sub_type_index);

				if (sub_type_index == g_array_index(priv->header_index_type, gint, index_subtype)) {
					e_msg_composer_modify_header (composer, (temp_header_ptr->header_type_value)->str,
						(temp_header_value_ptr->sub_header_string_value)->str);
				}
			}
		}
	}
}

static void
epech_custom_header_options_commit (EMsgComposer *comp, gpointer user_data)
{
        EMsgComposer *composer;
        EmailCustomHeaderWindow *new_email_custom_header_window = NULL;
        CustomHeaderOptionsDialog *current_dialog = NULL;

        composer = (EMsgComposer *) user_data;

        if (!user_data || !EMAIL_CUSTOM_HEADER_OPTIONS_IS_DIALOG (user_data))
		return;

        new_email_custom_header_window = g_object_get_data ((GObject *) composer, "compowindow");

        if (new_email_custom_header_window) {
		current_dialog = new_email_custom_header_window->epech_dialog;
        }

        if (current_dialog) {
		g_free (current_dialog);
		current_dialog = NULL;
        }

	if (new_email_custom_header_window) {
		g_free (new_email_custom_header_window);
		new_email_custom_header_window = NULL;
	}
}

static gint
epech_check_existing_composer_window(gconstpointer compowindow, gconstpointer other_compowindow)
{
	if ((compowindow) && (other_compowindow)) {
		if (((EmailCustomHeaderWindow *)compowindow)->epech_window == (GdkWindow *)other_compowindow) {
			return 0;
		}
	}

	return -1;
}

static void
destroy_compo_data (gpointer data)
{
        EmailCustomHeaderWindow *compo_data = (EmailCustomHeaderWindow *) data;

        if (!compo_data)
                return;

        g_free (compo_data);
}

static void action_email_custom_header_cb (GtkAction *action, EMsgComposer *composer)

{
	GtkUIManager *ui_manager;
	GtkWidget *menuitem;
	GdkWindow *window;
	CustomHeaderOptionsDialog *dialog = NULL;
	EmailCustomHeaderWindow *new_email_custom_header_window = NULL;

	ui_manager = gtkhtml_editor_get_ui_manager (GTKHTML_EDITOR (composer));
	menuitem = gtk_ui_manager_get_widget (ui_manager, "/main-menu/insert-menu/insert-menu-top/Custom Header");

	new_email_custom_header_window = g_object_get_data ((GObject *) composer, "compowindow");

	window = gtk_widget_get_window (menuitem);
	if (epech_check_existing_composer_window(new_email_custom_header_window,window) == 0) {
		dialog = new_email_custom_header_window->epech_dialog;
	} else {
		dialog = epech_dialog_new ();
		if (dialog) {
                        EmailCustomHeaderWindow *new_email_custom_header_window;
                        new_email_custom_header_window = g_new0(EmailCustomHeaderWindow, 1);
                        new_email_custom_header_window->epech_window = window;
                        new_email_custom_header_window->epech_dialog = dialog;
                        g_object_set_data_full ((GObject *) composer, "compowindow", new_email_custom_header_window, destroy_compo_data);
		}
	}

	epech_dialog_run (dialog, GTK_WIDGET (composer));
	g_signal_connect (dialog, "emch_response", G_CALLBACK (epech_append_to_custom_header), GTK_WIDGET (composer));
	g_signal_connect (GTK_WIDGET (composer), "destroy", G_CALLBACK (epech_custom_header_options_commit), composer);
}

static GtkActionEntry entries[] = {

	{ "Custom Header",
	  NULL,
	  N_("_Custom Header"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_email_custom_header_cb) }
};

gboolean
e_plugin_ui_init (GtkUIManager *ui_manager,
                  EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	editor = GTKHTML_EDITOR (composer);

	/* Add actions to the "composer" action group. */
	gtk_action_group_add_actions (
		gtkhtml_editor_get_action_group (editor, "composer"),
		entries, G_N_ELEMENTS (entries), composer);

	return TRUE;
}

static void
commit_changes (ConfigData *cd)
{
	GtkTreeModel *model = NULL;
	GSList *header_config_list = NULL;
	GtkTreeIter iter;
	gboolean valid;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (cd->treeview));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gchar *keyword = NULL, *value = NULL;

		gtk_tree_model_get (model, &iter,
			HEADER_KEY_COLUMN, &keyword,
			HEADER_VALUE_COLUMN, &value,
			-1);

                /* Check if the keyword is not empty */
                if ((keyword) && (g_utf8_strlen(g_strstrip(keyword), -1) > 0)) {
                        if ((value) && (g_utf8_strlen(g_strstrip(value), -1) > 0)) {
                                keyword = g_strconcat (keyword, "=", value, NULL);
                        }
                        header_config_list = g_slist_append (header_config_list, g_strdup(keyword));
                }

		g_free (keyword);
		g_free (value);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	gconf_client_set_list (cd->gconf, GCONF_KEY_CUSTOM_HEADER, GCONF_VALUE_STRING, header_config_list, NULL);

	g_slist_foreach (header_config_list, (GFunc) g_free, NULL);
	g_slist_free (header_config_list);
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
header_add_clicked (GtkButton *button, ConfigData *cd)
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
header_remove_clicked (GtkButton *button, ConfigData *cd)
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
		valid = gtk_tree_path_prev(path);

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	len = gtk_tree_model_iter_n_children (model, NULL);
	if (len > 0) {
		if (gtk_list_store_iter_is_valid (GTK_LIST_STORE(model), &iter)) {
			gtk_tree_selection_select_iter (selection, &iter);
		} else {
			if (path && valid) {
				gtk_tree_model_get_iter(model, &iter, path);
				gtk_tree_selection_select_iter (selection, &iter);
			}
		}
	} else {
		gtk_widget_set_sensitive (cd->header_edit, FALSE);
		gtk_widget_set_sensitive (cd->header_remove, FALSE);
	}

	gtk_widget_grab_focus(cd->treeview);
	gtk_tree_path_free (path);

	commit_changes (cd);
}

static void
header_edit_clicked (GtkButton *button, ConfigData *cd)
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
selection_changed (GtkTreeSelection *selection, ConfigData *cd)
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

	if (!cd)
		return;

	g_object_unref (cd->gconf);
	g_free (cd);
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkWidget *hbox;
	GSList *list;
	GSList *header_list = NULL;
	gint index;
	gchar *buffer;
	GtkTreeViewColumn *col;
	gint col_pos;
	GConfClient *client = gconf_client_get_default();
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

	ech_configuration_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (ech_configuration_box);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (ech_configuration_box), vbox2, FALSE, FALSE, 0);

	/* To translators: This string is used while adding a new message header to configuration, to specifying the format of the key values */
	label1 = gtk_label_new (_("The format for specifying a Custom Header key value is:\nName of the Custom Header key values separated by \";\"."));
	gtk_widget_show (label1);
	gtk_box_pack_start (GTK_BOX (vbox2), label1, FALSE, TRUE, 0);
	gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap (GTK_LABEL (label1), TRUE);

	header_configuration_box = gtk_vbox_new (FALSE, 5);
	gtk_widget_show (header_configuration_box);
	gtk_box_pack_start (GTK_BOX (ech_configuration_box), header_configuration_box, TRUE, TRUE, 0);

	header_container = gtk_hbox_new (FALSE, 6);
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

	vbuttonbox1 = gtk_vbutton_box_new ();
	gtk_widget_show (vbuttonbox1);
	gtk_box_pack_start (GTK_BOX (header_container), vbuttonbox1, FALSE, TRUE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox1), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox1), 6);

	header_add = gtk_button_new_from_stock ("gtk-add");
	gtk_widget_show (header_add);
	gtk_container_add (GTK_CONTAINER (vbuttonbox1), header_add);
	gtk_widget_set_can_default (header_add, TRUE);

	header_edit = gtk_button_new_from_stock ("gtk-edit");
	gtk_widget_show (header_edit);
	gtk_container_add (GTK_CONTAINER (vbuttonbox1), header_edit);
	gtk_widget_set_can_default (header_edit, TRUE);

	header_remove = gtk_button_new_from_stock ("gtk-remove");
	gtk_widget_show (header_remove);
	gtk_container_add (GTK_CONTAINER (vbuttonbox1), header_remove);
	gtk_widget_set_can_default (header_remove, TRUE);

	cd->gconf = gconf_client_get_default ();

	cd->treeview = header_treeview;

	cd->store = gtk_list_store_new (HEADER_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (cd->treeview), GTK_TREE_MODEL (cd->store));

	renderer = gtk_cell_renderer_text_new ();
	col_pos = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (cd->treeview), -1, _("Key"),
			renderer, "text", HEADER_KEY_COLUMN, NULL);
	col = gtk_tree_view_get_column (GTK_TREE_VIEW (cd->treeview), col_pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_object_set (col, "min-width", 50, NULL);

	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (cell_edited_cb), cd);
	g_signal_connect (
		renderer, "editing-canceled",
		G_CALLBACK (cell_editing_canceled_cb), cd);

	renderer = gtk_cell_renderer_text_new ();
	col_pos = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (cd->treeview), -1, _("Values"),
			renderer, "text", HEADER_VALUE_COLUMN, NULL);
	col = gtk_tree_view_get_column (GTK_TREE_VIEW (cd->treeview), col_pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);

	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (cell_value_edited_cb), cd);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cd->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (selection_changed), cd);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (cd->treeview), TRUE);

	cd->header_add = header_add;
	g_signal_connect (G_OBJECT (cd->header_add), "clicked", G_CALLBACK (header_add_clicked), cd);

	cd->header_remove = header_remove;
	g_signal_connect (G_OBJECT (cd->header_remove), "clicked", G_CALLBACK (header_remove_clicked), cd);
	gtk_widget_set_sensitive (cd->header_remove, FALSE);

	cd->header_edit = header_edit;
	g_signal_connect (G_OBJECT (cd->header_edit), "clicked", G_CALLBACK (header_edit_clicked), cd);
	gtk_widget_set_sensitive (cd->header_edit, FALSE);

	/* Populate tree view with values from gconf */
	header_list = gconf_client_get_list (client,GCONF_KEY_CUSTOM_HEADER,GCONF_VALUE_STRING, NULL);

	for (list = header_list; list; list = g_slist_next (list)) {
		gchar **parse_header_list;

		buffer = list->data;
		gtk_list_store_append (cd->store, &iter);

		parse_header_list = g_strsplit_set (buffer, "=,", -1);

		gtk_list_store_set (cd->store, &iter, HEADER_KEY_COLUMN, parse_header_list[0], -1);

		for (index = 0; parse_header_list[index+1] ; ++index) {
			gtk_list_store_set (cd->store, &iter, HEADER_VALUE_COLUMN, parse_header_list[index+1], -1);
		}
	}

	if (header_list) {
		g_slist_foreach (header_list, (GFunc) g_free, NULL);
		g_slist_free (header_list);
	}

	/* Add the list here */

	hbox = gtk_vbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), ech_configuration_box, TRUE, TRUE, 0);

	/* to let free data properly on destroy of configuration widget */
	g_object_set_data_full (G_OBJECT (hbox), "mycd-data", cd, destroy_cd_data);

	return hbox;
}

/* Configuration in Mail Prefs Page goes here */

GtkWidget *
org_gnome_email_custom_header_config_option (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data)
{
	/* This function and the hook needs to be removed,
	once the configure code is thoroughly tested */

	return NULL;

}
