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
 *		R.Raghavendran <raghavguru7@gmail.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <libedataserverui/e-name-selector.h>
#include <libedataserverui/e-contact-store.h>
#include "exchange-operations.h"
#include <e-util/e-util.h>
#include <e-util/e-error.h>
#include <glade/glade.h>

#include "e-util/e-util-private.h"

#include "exchange-send-options.h"

struct _ExchangeSendOptionsDialogPrivate {
	/* Glade XML data */
	GladeXML *xml;

	/*Widgets*/

	GtkWidget *main;

	/*name selector dialog*/
	ENameSelector *proxy_name_selector;

	/*Importance*/
	GtkWidget *importance;

	/*Sensitivity*/
	GtkWidget *sensitivity;

	/*Send_as_delegate_enabled*/
	GtkWidget *delegate_enabled;

	/*Read Receipt*/
	GtkWidget *read_receipt;

	/*Delivery Receipt*/
	GtkWidget *delivery_receipt;

	/*User button*/
	GtkWidget *button_user;

	/*Label Widgets*/
	GtkWidget *importance_label;
	GtkWidget *sensitivity_label;
	gchar *help_section;
};

static void exchange_sendoptions_dialog_class_init (GObjectClass *object_class);
static void exchange_sendoptions_dialog_finalize (GObject *object);
static void exchange_sendoptions_dialog_init (GObject *object);
static void exchange_sendoptions_dialog_dispose (GObject *object);

static GObjectClass *parent_class = NULL;
enum {
	SOD_RESPONSE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* exchange_send_options_get_widgets_data(ExchangeSendOptionsDialog *sod)
   Return Value:This function returns a -1 if an error occurs. In case of error-free operation a 1 is returned.
*/
static gint
exchange_send_options_get_widgets_data (ExchangeSendOptionsDialog *sod)
{
	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptions *options;
	const gchar *address, *email, *name;

	guint count=0;
	ENameSelectorEntry *name_selector_entry;
	EDestinationStore *destination_store;
	GList *destinations, *tmp;

	priv = sod->priv;
	options = sod->options;

	/* This block helps us fetch the address of the delegator(s). If no delegator is selected or more
	   than one delegatee has been selected then an info dialog is popped up to help the user.
	*/
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->delegate_enabled))) {

		name_selector_entry = e_name_selector_peek_section_entry (priv->proxy_name_selector, "Add User");
		destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY									       (name_selector_entry));
		destinations = e_destination_store_list_destinations (destination_store);
		tmp = destinations;

		email = NULL;

		/* The temporary variables address, email, and name are needed to fetch the list items.
		   Only the valid one is then copied into the storage variables. The "count" variable
		   helps us keep a count of the exact number of items in the list. The g_list_length(GList *)
		   produced ambiguous results. Hence count is used :)
		*/
		for (; tmp != NULL; tmp = g_list_next (tmp)) {
			address = g_strdup ((gchar *) e_destination_get_address (tmp->data));
			email = g_strdup ((gchar *) e_destination_get_email (tmp->data));
			name = g_strdup (e_destination_get_name (tmp->data));
			if (g_str_equal (email, ""))
				continue;
			count++;

			options->delegate_address = address;
			options->delegate_name = name;
			options->delegate_email = email;
		}

		if (count == 0) {
			e_error_run ((GtkWindow *) priv->main,
				"org-gnome-exchange-operations:no-delegate-selected", NULL, NULL);
			gtk_widget_grab_focus ((GtkWidget *) name_selector_entry);
			options->delegate_address = NULL;
			options->delegate_name = NULL;
			options->delegate_email = NULL;
			return -1;
		}

		if (count > 1) {
			e_error_run ((GtkWindow *)priv->main,
				"org-gnome-exchange-operations:more-delegates-selected", NULL, NULL);
			gtk_widget_grab_focus ((GtkWidget *) name_selector_entry);
			options->delegate_address = NULL;
			options->delegate_name = NULL;
			options->delegate_email = NULL;
			return -1;
		}
	}

	options->importance = gtk_combo_box_get_active ((GtkComboBox *)priv->importance);
	options->sensitivity = gtk_combo_box_get_active ((GtkComboBox *)priv->sensitivity);

	options->send_as_del_enabled = gtk_toggle_button_get_active (
					GTK_TOGGLE_BUTTON (priv->delegate_enabled));

	options->delivery_enabled = gtk_toggle_button_get_active (
					GTK_TOGGLE_BUTTON (priv->delivery_receipt));

	options->read_enabled = gtk_toggle_button_get_active (
					GTK_TOGGLE_BUTTON (priv->read_receipt));

	return 1;
}

static gboolean
get_widgets (ExchangeSendOptionsDialog *sod)
{
	ExchangeSendOptionsDialogPrivate *priv;

	priv = sod->priv;

#define EXCHANGE(name) glade_xml_get_widget (priv->xml, name)

	priv->main = EXCHANGE ("send_options");
	if (!priv->main)
		return FALSE;

	priv->importance = EXCHANGE ("imp_combo_box");
	priv->sensitivity = EXCHANGE ("sensitivity_combo_box");
	priv->button_user = EXCHANGE ("button-user");
	priv->delegate_enabled = EXCHANGE ("del_enabled_check");
	priv->read_receipt = EXCHANGE ("read_check_button");
	priv->delivery_receipt = EXCHANGE ("delivery_check_button");
	priv->importance_label = EXCHANGE ("Importance_label");
	priv->sensitivity_label = EXCHANGE ("Sensitivity_label");

#undef EXCHANGE

	return (priv->importance
		&&priv->sensitivity
		&&priv->button_user
		&&priv->delegate_enabled
		&&priv->read_receipt
		&&priv->delivery_receipt
		&&priv->importance_label
		&&priv->sensitivity_label);
}

static void
exchange_send_options_fill_widgets_with_data (ExchangeSendOptionsDialog *sod)
{
	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptions *options;
	ENameSelectorEntry *name_selector_entry;

	priv = sod->priv;
	options = sod->options;

	priv->help_section = g_strdup ("usage-mail");

	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->importance_label), priv->importance);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->sensitivity_label), priv->sensitivity);

	gtk_combo_box_set_active ((GtkComboBox *) priv->importance, options->importance);
	gtk_combo_box_set_active ((GtkComboBox *) priv->sensitivity, options->sensitivity);

	name_selector_entry = e_name_selector_peek_section_entry (priv->proxy_name_selector, "Add User");

	if (options->send_as_del_enabled) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delegate_enabled), TRUE);
		gtk_widget_set_sensitive ((GtkWidget *)name_selector_entry, TRUE);
		gtk_widget_set_sensitive ((GtkWidget *)priv->button_user, TRUE);
	}

	else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delegate_enabled), FALSE);
		gtk_widget_set_sensitive ((GtkWidget *)name_selector_entry, FALSE);
		gtk_widget_set_sensitive ((GtkWidget *)priv->button_user, FALSE);
	}

	if (options->read_enabled)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->read_receipt), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->read_receipt), FALSE);

	if (options->delivery_enabled)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delivery_receipt), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delivery_receipt), FALSE);
}

ExchangeSendOptionsDialog *
exchange_sendoptions_dialog_new (void) {
	ExchangeSendOptionsDialog *sod;

	sod = g_object_new (EXCHANGE_TYPE_SENDOPTIONS_DIALOG, NULL);

	return sod;
}

GType exchange_sendoptions_dialog_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (ExchangeSendOptionsDialogClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc) exchange_sendoptions_dialog_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (ExchangeSendOptionsDialog),
     0,      /* n_preallocs */
     (GInstanceInitFunc) exchange_sendoptions_dialog_init,
	NULL    /* instance_init */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "ExchangeSendOptionsDialogType",
                                   &info, 0);
  }
  return type;
}

static void exchange_send_options_cb (GtkDialog *dialog, gint state, gpointer func_data)
{
	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptionsDialog *sod;

	sod = func_data;
        priv = sod->priv;

	switch (state) {
		case GTK_RESPONSE_OK:
		     if (exchange_send_options_get_widgets_data (sod) < 0)
			return;
		case GTK_RESPONSE_CANCEL:
			gtk_widget_hide (priv->main);
			gtk_widget_destroy (priv->main);
			g_object_unref (priv->xml);
			break;
		case GTK_RESPONSE_HELP:
			e_display_help (
				GTK_WINDOW (priv->main),
				priv->help_section);
			break;
	}
	g_signal_emit (G_OBJECT (func_data), signals[SOD_RESPONSE], 0, state);

}

/* This function acts as a listener for the toggling of "send_as_a_delegate" button. This is needed to
   sensitize the name_selector_entry and the User Button
*/
static void
delegate_option_toggled (GtkCheckButton *button, gpointer func_data)
{
	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptionsDialog *sod;
	ENameSelectorEntry *name_selector_entry;
	ExchangeSendOptions *options;

	sod=func_data;
	priv=sod->priv;
	options=sod->options;

	name_selector_entry = e_name_selector_peek_section_entry (priv->proxy_name_selector, "Add User");

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->delegate_enabled))) {
		gtk_widget_set_sensitive ((GtkWidget *) name_selector_entry, TRUE);
		gtk_widget_set_sensitive ((GtkWidget *) priv->button_user, TRUE);
	}

	else {
		gtk_widget_set_sensitive ((GtkWidget *) name_selector_entry, FALSE);
		gtk_widget_set_sensitive ((GtkWidget *) priv->button_user, FALSE);
	}

}

static void
addressbook_dialog_response (ENameSelectorDialog *name_selector_dialog, gint response, gpointer user_data)
{
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

static void
addressbook_entry_changed (GtkWidget *entry, gpointer user_data)
{
}

/* This function invokes the name selector dialog
*/
static void
address_button_clicked (GtkButton *button, gpointer func_data)
{

	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptionsDialog *sod;
	ENameSelectorDialog *name_selector_dialog;

	sod=func_data;
	priv=sod->priv;

	name_selector_dialog = e_name_selector_peek_dialog (priv->proxy_name_selector);
	gtk_widget_show (GTK_WIDGET (name_selector_dialog));
}

gboolean
exchange_sendoptions_dialog_run (ExchangeSendOptionsDialog *sod, GtkWidget *parent)
{
	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptions *options;

	GtkWidget *toplevel;
	gchar *filename;
	EDestinationStore *destination_store;
	ENameSelectorDialog *name_selector_dialog;
	ENameSelectorModel *name_selector_model;
	ENameSelectorEntry *name_selector_entry;
	EDestination *des;
	GtkWidget *name_box;

	g_return_val_if_fail (sod != NULL || EXCHANGE_IS_SENDOPTIONS_DIALOG (sod), FALSE);

	priv = sod->priv;
	options = sod->options;

	filename = g_build_filename (EVOLUTION_GLADEDIR,
				     "exchange-send-options.glade",
				     NULL);
	priv->xml = glade_xml_new (filename, NULL, NULL);
	g_free (filename);

	if (!priv->xml) {
		g_message ( G_STRLOC ": Could not load the Glade XML file ");
		return FALSE;
	}

	if (!get_widgets(sod)) {
		g_object_unref (priv->xml);
		g_message (G_STRLOC ": Could not get the Widgets \n");
		return FALSE;
	}

	toplevel =  gtk_widget_get_toplevel (priv->main);
	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (toplevel),
				      GTK_WINDOW (parent));

	priv->proxy_name_selector = e_name_selector_new ();
	name_selector_dialog = e_name_selector_peek_dialog (priv->proxy_name_selector);

	name_selector_model = e_name_selector_peek_model (priv->proxy_name_selector);
	e_name_selector_model_add_section (name_selector_model, "Add User", _("Add User"), NULL);

	exchange_send_options_fill_widgets_with_data (sod);

	if (options->delegate_address) {
		e_name_selector_model_peek_section (name_selector_model, "Add User", NULL, &destination_store);
		des = e_destination_new ();
		e_destination_set_email (des, options->delegate_email);
		e_destination_set_name (des, options->delegate_name);
		e_destination_store_append_destination (destination_store, des);
		g_object_unref (des);
	}

	g_signal_connect ((GtkButton *) priv->button_user, "clicked",
				G_CALLBACK (address_button_clicked), sod);
	g_signal_connect (name_selector_dialog, "response",
				G_CALLBACK (addressbook_dialog_response), sod);
	g_signal_connect (GTK_DIALOG (priv->main), "response",
				G_CALLBACK(exchange_send_options_cb), sod);
	g_signal_connect ((GtkCheckButton *) priv->delegate_enabled, "toggled",
				G_CALLBACK(delegate_option_toggled), sod);

	name_selector_entry = e_name_selector_peek_section_entry (priv->proxy_name_selector,
									"Add User");
	g_signal_connect (name_selector_entry, "changed", G_CALLBACK (addressbook_entry_changed), sod);

	/* The name box is just a container. The name_selector_entry is added to it. This Widget
	   is created dynamically*/
	name_box = glade_xml_get_widget (priv->xml, "del_name_box");
	gtk_container_add ((GtkContainer *) name_box, (GtkWidget *) name_selector_entry);
	gtk_widget_show ((GtkWidget *) name_selector_entry);
	gtk_widget_grab_focus ((GtkWidget *) name_selector_entry);

	gtk_window_set_modal ((GtkWindow *) priv->main, TRUE);
	gtk_widget_show (priv->main);

	return TRUE;
}

static void
exchange_sendoptions_dialog_class_init (GObjectClass *object)
{
	ExchangeSendOptionsDialogClass *klass;
	GObjectClass *object_class;

	klass = EXCHANGE_SENDOPTIONS_DIALOG_CLASS (object);
	parent_class = g_type_class_peek_parent (klass);
	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = exchange_sendoptions_dialog_finalize;
	object_class->dispose = exchange_sendoptions_dialog_dispose;
	signals[SOD_RESPONSE] = g_signal_new ("sod_response",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (ExchangeSendOptionsDialogClass, esod_response),
			NULL, NULL,
			g_cclosure_marshal_VOID__INT,
			G_TYPE_NONE, 1,
			G_TYPE_INT);

}

static void
exchange_sendoptions_dialog_init (GObject *object)
{

	ExchangeSendOptionsDialog *sod;
	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptions *new;

	sod = EXCHANGE_SENDOPTIONS_DIALOG (object);
	new = g_new0 (ExchangeSendOptions, 1);

	priv = g_new0 (ExchangeSendOptionsDialogPrivate, 1);

	sod->priv = priv;
	sod->options = new;
	sod->options->send_as_del_enabled = FALSE;
	sod->options->delivery_enabled = FALSE;
	sod->options->read_enabled = FALSE;
	sod->options->importance = E_IMP_NORMAL;
	sod->options->sensitivity = E_SENSITIVITY_NORMAL;

	priv->xml = NULL;
	priv->main = NULL;
	priv->importance = NULL;
	priv->sensitivity = NULL;
	priv->sensitivity_label = NULL;
	priv->importance_label = NULL;
	priv->button_user = NULL;
	priv->proxy_name_selector = NULL;
	priv->read_receipt = NULL;
	priv->delivery_receipt = NULL;

}

static void
exchange_sendoptions_dialog_finalize (GObject *object)
{
	ExchangeSendOptionsDialog *sod = (ExchangeSendOptionsDialog *)object;
	ExchangeSendOptionsDialogPrivate *priv;

	g_return_if_fail (EXCHANGE_IS_SENDOPTIONS_DIALOG (sod));
	priv = sod->priv;

	g_free (priv->help_section);

	if (sod->options) {
		g_free (sod->options);
		sod->options = NULL;
	}

	if (sod->priv) {
		g_free (sod->priv);
		sod->priv = NULL;
	}

	if (parent_class->finalize)
		(* parent_class->finalize) (object);

}

static void
exchange_sendoptions_dialog_dispose (GObject *object)
{
	ExchangeSendOptionsDialog *sod = (ExchangeSendOptionsDialog *) object;

	g_return_if_fail (EXCHANGE_IS_SENDOPTIONS_DIALOG (sod));

	if (parent_class->dispose)
		(* parent_class->dispose) (object);

}

