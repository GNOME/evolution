/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Ashish Shrivastava <shashish@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, MA 02110-1301.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <e-util/e-error.h>
#include <libgnome/libgnome.h>
#include <glade/glade.h>
#include "mail/em-menu.h"
#include "mail/em-utils.h"
#include "mail/em-event.h"
#include "composer/e-msg-composer.h"
#include "libedataserver/e-account.h"
#include "email-custom-header.h"


#define d(x) x

struct _EmailCustomHeaderOptionsDialogPrivate {
	/* Glade XML data */
	GladeXML *xml;
	/*Widgets*/
	GtkWidget *main;
	GtkWidget *page;
	GtkWidget *header_table;
	GtkWidget *header_type_name_label;
	GArray *combo_box_header_value;
	GArray *email_custom_header_details;
	GArray *header_index_type;
	gint flag;
	char *help_section;
};

/* epech - e-plugin email custom header*/
static void epech_dialog_class_init (GObjectClass *object_class);
static void epech_dialog_finalize (GObject *object);
static void epech_dialog_init (GObject *object);
static void epech_dialog_dispose (GObject *object);
static void epech_setup_widgets (CustomHeaderOptionsDialog *mch);
static gint epech_check_existing_composer_window(gconstpointer a, gconstpointer b);

gboolean e_plugin_ui_init(GtkUIManager *manager, EMsgComposer *composer);

static void 
epech_get_widgets_data (CustomHeaderOptionsDialog *mch)
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	HeaderValueComboBox *sub_combo_box_get;
	gint selected_item;
	gint index_row,index_column;

	priv = mch->priv;
	priv->header_index_type = g_array_new (FALSE, FALSE, sizeof (gint));
	priv->flag++;

	for (index_row = 0,index_column = 0;
		index_column < priv->email_custom_header_details->len; index_column++) {

		sub_combo_box_get = &g_array_index(priv->combo_box_header_value, HeaderValueComboBox,index_column);
		selected_item = gtk_combo_box_get_active((GtkComboBox *)sub_combo_box_get->header_value_combo_box);
		g_array_append_val (priv->header_index_type, selected_item);
	}

	return;
}

static gboolean 
epech_get_widgets (CustomHeaderOptionsDialog *mch) 
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	priv = mch->priv;

#define EMAIL_CUSTOM_HEADER(name) glade_xml_get_widget (priv->xml, name)
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
	gint set_index_row,set_index_column;	

	priv = mch->priv;
	priv->help_section = g_strdup ("usage-mail");

	for (set_index_row = 0,set_index_column = 0; 
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
	GError *error = NULL;     

	mch = func_data;
	priv = mch->priv;

	switch (state) {			
		case GTK_RESPONSE_OK:
			epech_get_widgets_data (mch); 
		case GTK_RESPONSE_CANCEL:
			gtk_widget_hide (priv->main);
			gtk_widget_destroy (priv->main);
			g_object_unref (priv->xml);
			break;	     
		case GTK_RESPONSE_HELP:
			gnome_help_display (
					"evolution.xml", priv->help_section, &error);
			if (error) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
			break;
	}

	g_signal_emit (G_OBJECT (func_data), signals[MCH_RESPONSE], 0, state);
}

static gboolean 
epech_dialog_run (CustomHeaderOptionsDialog *mch, GtkWidget *parent)
{	
	EmailCustomHeaderOptionsDialogPrivate *priv;
	GtkWidget *toplevel;
	gchar *filename;

	g_return_val_if_fail (mch != NULL || EMAIL_CUSTOM_HEADER_OPTIONS_IS_DIALOG (mch), FALSE);
	priv = mch->priv;
	epech_get_header_list (mch);

	filename = g_build_filename (EVOLUTION_GLADEDIR,
			"org-gnome-email-custom-header.glade",
			NULL);
	priv->xml = glade_xml_new (filename, NULL, NULL);
	g_free (filename);

	if (!priv->xml) {
		d (printf ("\n Could not load the Glade XML file\n"));
	}

	if (!epech_get_widgets(mch)) {
		g_object_unref (priv->xml);
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
	gconf_client_add_dir (client, "/apps/evolution/eplugin/email_custom_header" , GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	epech_load_from_gconf (client, "/apps/evolution/eplugin/email_custom_header/customHeader", mch);

	return;
}

static void 
epech_load_from_gconf (GConfClient *client,const char *path,CustomHeaderOptionsDialog *mch)
{
	EmailCustomHeaderOptionsDialogPrivate *priv;
	EmailCustomHeaderDetails temp_header_details= {-1, -1, NULL, NULL};
	CustomSubHeader temp_header_value_details =  {NULL};
	GSList *header_list,*q;
	gchar *buffer;
	char *str_colon;
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
		str_colon = g_strconcat (parse_header_list[0], ":", NULL);
		temp_header_details.header_type_value = g_string_new("");
		if (temp_header_details.header_type_value) {
			g_string_assign(temp_header_details.header_type_value, str_colon);
		}

		g_free (str_colon);
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
	gint header_section_id,sub_type_index,row,column,label_row;

	priv = mch->priv;
	priv->combo_box_header_value = g_array_new (TRUE, FALSE, sizeof (HeaderValueComboBox)); 

	for (header_section_id = 0,label_row = 0,row = 0,column = 1; 
		header_section_id < priv->email_custom_header_details->len; header_section_id++,row++,column++) {

		// To create an empty label widget. Text will be added dynamically.
		priv->header_type_name_label = gtk_label_new ("");
		temp_header_ptr = &g_array_index(priv->email_custom_header_details, EmailCustomHeaderDetails,header_section_id);
		gtk_label_set_markup (GTK_LABEL (priv->header_type_name_label),(temp_header_ptr->header_type_value)->str);

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
			gtk_combo_box_append_text (GTK_COMBO_BOX (sub_combo_box_ptr->header_value_combo_box),
				(temp_header_value_ptr->sub_header_string_value)->str);
		}

		gtk_combo_box_append_text (GTK_COMBO_BOX (sub_combo_box_ptr->header_value_combo_box),"None");
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
	priv->xml = NULL;
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
	gint index, index_subtype,sub_type_index;

	composer = (EMsgComposer *)data;
	priv = dialog->priv;

	if (state == GTK_RESPONSE_OK) {

		for (index = 0,index_subtype = 0; index_subtype < priv->email_custom_header_details->len; index_subtype++) {

			temp_header_ptr = &g_array_index(priv->email_custom_header_details, EmailCustomHeaderDetails,index_subtype);

			for (sub_type_index = 0; sub_type_index < temp_header_ptr->number_of_subtype_header; sub_type_index++) {
				temp_header_value_ptr = &g_array_index(temp_header_ptr->sub_header_type_value, CustomSubHeader,sub_type_index);

				if (sub_type_index == g_array_index(priv->header_index_type, gint, index_subtype)){
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
	if ((compowindow) && (other_compowindow)){
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
	GtkUIManager  *manager;
	GtkWidget     *menuitem;
	CustomHeaderOptionsDialog *dialog = NULL;
	EmailCustomHeaderWindow *new_email_custom_header_window = NULL;

	manager = gtkhtml_editor_get_ui_manager (GTKHTML_EDITOR (composer));
	menuitem = gtk_ui_manager_get_widget (manager, "/main-menu/insert-menu/insert-menu-top/Custom Header");

	new_email_custom_header_window = g_object_get_data ((GObject *) composer, "compowindow");

	if (epech_check_existing_composer_window(new_email_custom_header_window,menuitem->window) == 0) {
		dialog = new_email_custom_header_window->epech_dialog;
	} else {
		dialog = epech_dialog_new ();
		if (dialog) {
                        EmailCustomHeaderWindow *new_email_custom_header_window;
                        new_email_custom_header_window = g_new0(EmailCustomHeaderWindow, 1);
                        new_email_custom_header_window->epech_window =  menuitem->window;
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
e_plugin_ui_init (GtkUIManager *manager,
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

