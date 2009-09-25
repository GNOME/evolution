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

#include "e-util/e-util-private.h"

#include "exchange-send-options.h"

struct _ExchangeSendOptionsDialogPrivate {
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

	EDestinationStore *destination_store;
	ENameSelectorDialog *name_selector_dialog;
	ENameSelectorModel *name_selector_model;
	ENameSelectorEntry *name_selector_entry;
	EDestination *des;
	GtkWidget *send_options;
	GtkWidget *send_options_vbox;
	GtkWidget *options_vbox;
	GtkWidget *message_settings_vbox;
	GtkWidget *msg_settings_label;
	GtkWidget *msg_settings_table;
	GtkWidget *importance_label;
	GtkWidget *sensitivity_label;
	GtkWidget *sensitivity_combo_box;
	GtkWidget *imp_combo_box;
	GtkWidget *del_enabled_check;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *del_name_box;
	GtkWidget *button_user;
	GtkWidget *track_option_vbox;
	GtkWidget *track_options_label;
	GtkWidget *delivery_check_button;
	GtkWidget *read_check_button;
	gchar *tmp_str;

	g_return_val_if_fail (sod != NULL || EXCHANGE_IS_SENDOPTIONS_DIALOG (sod), FALSE);

	priv = sod->priv;
	options = sod->options;

	send_options = gtk_dialog_new_with_buttons (
		_("Exchange - Send Options"),
		NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_HELP, GTK_RESPONSE_HELP,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_type_hint (GTK_WINDOW (send_options), GDK_WINDOW_TYPE_HINT_DIALOG);

	send_options_vbox = gtk_dialog_get_content_area (GTK_DIALOG (send_options));
	gtk_widget_show (send_options_vbox);

	options_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (options_vbox);
	gtk_box_pack_start (GTK_BOX (send_options_vbox), options_vbox, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (options_vbox), 6);

	message_settings_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (message_settings_vbox);
	gtk_box_pack_start (GTK_BOX (options_vbox), message_settings_vbox, FALSE, FALSE, 0);

	tmp_str = g_strconcat ("<b>", _("Message Settings"), "</b>", NULL);
	msg_settings_label = gtk_label_new (tmp_str);
	g_free (tmp_str);
	gtk_widget_show (msg_settings_label);
	gtk_box_pack_start (GTK_BOX (message_settings_vbox), msg_settings_label, FALSE, FALSE, 0);
	gtk_label_set_use_markup (GTK_LABEL (msg_settings_label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (msg_settings_label), 0, 0.49);

	msg_settings_table = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (msg_settings_table);
	gtk_box_pack_start (GTK_BOX (message_settings_vbox), msg_settings_table, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (msg_settings_table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (msg_settings_table), 6);

	importance_label = gtk_label_new_with_mnemonic (_("I_mportance: "));
	gtk_widget_show (importance_label);
	gtk_table_attach (GTK_TABLE (msg_settings_table), importance_label, 0, 1, 0, 1,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (importance_label), 0, 0.49);

	sensitivity_label = gtk_label_new_with_mnemonic (_("_Sensitivity: "));
	gtk_widget_show (sensitivity_label);
	gtk_table_attach (GTK_TABLE (msg_settings_table), sensitivity_label, 0, 1, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (sensitivity_label), 0, 0.5);

	sensitivity_combo_box = gtk_combo_box_new_text ();
	gtk_widget_show (sensitivity_combo_box);
	gtk_table_attach (GTK_TABLE (msg_settings_table), sensitivity_combo_box, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_combo_box_append_text (GTK_COMBO_BOX (sensitivity_combo_box), _("Normal"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (sensitivity_combo_box), _("Personal"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (sensitivity_combo_box), _("Private"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (sensitivity_combo_box), _("Confidential"));

	imp_combo_box = gtk_combo_box_new_text ();
	gtk_widget_show (imp_combo_box);
	gtk_table_attach (GTK_TABLE (msg_settings_table), imp_combo_box, 1, 2, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_combo_box_append_text (GTK_COMBO_BOX (imp_combo_box), _("Normal"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (imp_combo_box), _("High"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (imp_combo_box), _("Low"));

	del_enabled_check = gtk_check_button_new_with_mnemonic (_("Send as Delegate"));
	gtk_widget_show (del_enabled_check);
	gtk_box_pack_start (GTK_BOX (options_vbox), del_enabled_check, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (del_enabled_check), 6);

	hbox1 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (options_vbox), hbox1, TRUE, TRUE, 0);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox2, TRUE, TRUE, 0);

	del_name_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (del_name_box);
	gtk_box_pack_start (GTK_BOX (hbox2), del_name_box, TRUE, TRUE, 0);

	button_user = gtk_button_new_with_mnemonic (_("_User"));
	gtk_widget_show (button_user);
	gtk_box_pack_start (GTK_BOX (hbox1), button_user, FALSE, FALSE, 0);

	track_option_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (track_option_vbox);
	gtk_box_pack_start (GTK_BOX (options_vbox), track_option_vbox, TRUE, TRUE, 0);

	tmp_str = g_strconcat ("<b>", _("Tracking Options"), "</b>", NULL);
	track_options_label = gtk_label_new (tmp_str);
	g_free (tmp_str);
	gtk_widget_show (track_options_label);
	gtk_box_pack_start (GTK_BOX (track_option_vbox), track_options_label, FALSE, FALSE, 6);
	gtk_label_set_use_markup (GTK_LABEL (track_options_label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (track_options_label), 0, 0.5);

	delivery_check_button = gtk_check_button_new_with_mnemonic (_("Request a _delivery receipt for this message"));
	gtk_widget_show (delivery_check_button);
	gtk_box_pack_start (GTK_BOX (track_option_vbox), delivery_check_button, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (delivery_check_button), 6);

	read_check_button = gtk_check_button_new_with_mnemonic (_("Request a _read receipt for this message"));
	gtk_widget_show (read_check_button);
	gtk_box_pack_start (GTK_BOX (track_option_vbox), read_check_button, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (read_check_button), 6);

	priv->main = send_options;
	priv->importance = imp_combo_box;
	priv->sensitivity = sensitivity_combo_box;
	priv->button_user = button_user;
	priv->delegate_enabled = del_enabled_check;
	priv->read_receipt = read_check_button;
	priv->delivery_receipt = delivery_check_button;
	priv->importance_label = importance_label;
	priv->sensitivity_label = sensitivity_label;

	send_options =  gtk_widget_get_toplevel (priv->main);
	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (send_options),
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
	gtk_container_add ((GtkContainer *) del_name_box, (GtkWidget *) name_selector_entry);
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

