/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Ashish Shrivastava <shashish@novell.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "composer/e-msg-composer.h"
#include "email-custom-header.h"

#define d(x)

#define ECM_SETTINGS_ID  "org.gnome.evolution.plugin.email-custom-header"
#define ECM_SETTINGS_KEY "custom-header"

/* EEmailCustomHeader extension */

typedef struct _EEmailCustomHeader EEmailCustomHeader;
typedef struct _EEmailCustomHeaderClass EEmailCustomHeaderClass;

struct _EEmailCustomHeader {
	EExtension parent;
};

struct _EEmailCustomHeaderClass {
	EExtensionClass parent_class;
};

GType e_email_custom_header_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EEmailCustomHeader, e_email_custom_header, E_TYPE_EXTENSION)

/* CustomHeaderOptionsDialog */

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

GType custom_header_options_dialog_get_type (void);
static void epech_dialog_finalize (GObject *object);
static void epech_setup_widgets (CustomHeaderOptionsDialog *mch);

G_DEFINE_TYPE_WITH_PRIVATE (CustomHeaderOptionsDialog, custom_header_options_dialog, G_TYPE_OBJECT)

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

static void
email_custom_header_setup (EMsgComposer *composer)
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
}

static void
e_email_custom_header_constructed (GObject *object)
{
	EExtension *extension = E_EXTENSION (object);
	EMsgComposer *composer;

	G_OBJECT_CLASS (e_email_custom_header_parent_class)->constructed (object);

	composer = E_MSG_COMPOSER (e_extension_get_extensible (extension));
	email_custom_header_setup (composer);
}

static void
e_email_custom_header_class_init (EEmailCustomHeaderClass *class)
{
	G_OBJECT_CLASS (class)->constructed = e_email_custom_header_constructed;
	E_EXTENSION_CLASS (class)->extensible_type = E_TYPE_MSG_COMPOSER;
}

static void
e_email_custom_header_class_finalize (EEmailCustomHeaderClass *class)
{
}

static void
e_email_custom_header_init (EEmailCustomHeader *extension)
{
}

void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_email_custom_header_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
