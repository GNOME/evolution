/* Evolution calendar - Main page of the Groupwise send options Dialog
 *
 * Copyright (C) 2004 Novell, Inc.
 *
 * Authors: Chenthill Palanisamy <pchenthill@novell.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Bangalore, MA 02111-1307, India.
 */

#include <string.h>
#include <glib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <time.h>
#include "e-dateedit.h"
#include "e-send-options.h"


struct _ESendOptionsDialogPrivate {
	/* Glade XML data */
	GladeXML *xml;

	gboolean gopts_needed;

	/* Widgets */

	GtkWidget *main;

	/* Noteboook to add options page and status tracking page*/
	GtkNotebook *notebook;

	/* priority */
	GtkWidget *priority;

	/* Classification */
	GtkWidget *classification;
		
	/* Widgets for Reply Requestion options */
	GtkWidget *reply_request;
	GtkWidget *reply_convenient;
	GtkWidget *reply_within;
	GtkWidget *within_days;

	/* Widgets for delay delivery Option */
	GtkWidget *delay_delivery;
	GtkWidget *delay_until;

	/* Widgets for Choosing expiration date */
	GtkWidget *expiration;
	GtkWidget *expire_after;

	/* Widgets to for tracking information through sent Item */
	GtkWidget *create_sent;
	GtkWidget *delivered;
	GtkWidget *delivered_opened;
	GtkWidget *all_info;
	GtkWidget *autodelete;

	/* Widgets for setting the Return Notification */
	GtkWidget *when_opened;
	GtkWidget *when_declined;
	GtkWidget *when_accepted;
	GtkWidget *when_completed;
	
	/* label widgets */
	GtkWidget *classification_label;
	GtkWidget *priority_label;
	GtkWidget *gopts_label;
	GtkWidget *sopts_label;
	GtkWidget *opened_label;
	GtkWidget *declined_label;
	GtkWidget *accepted_label;	
	GtkWidget *completed_label;
	GtkWidget *until_label;
};

static void e_sendoptions_dialog_class_init (GObjectClass *object_class);
static void e_sendoptions_dialog_finalize (GObject *object);
static void e_sendoptions_dialog_init (GObject *object);
static void e_sendoptions_dialog_dispose (GObject *object);


static void
e_send_options_get_widgets_data (ESendOptionsDialog *sod)
{
	ESendOptionsDialogPrivate *priv;
	ESendOptionsGeneral *gopts;
	ESendOptionsStatusTracking *sopts;
	
	priv = sod->priv;
	gopts = sod->data->gopts;
	sopts = sod->data->sopts;
	
	gopts->priority = gtk_combo_box_get_active ((GtkComboBox *) priv->priority);
		
	gopts->reply_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->reply_request));
	gopts->reply_convenient = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->reply_convenient));
	gopts->reply_within = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->within_days));
	
	gopts->expiration_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->expiration));
	gopts->expire_after = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->expire_after));
	gopts->delay_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->delay_delivery));
	
	if (e_date_edit_date_is_valid (E_DATE_EDIT (priv->delay_until)) && 
								e_date_edit_time_is_valid (E_DATE_EDIT(priv->delay_until)))
		gopts->delay_until = e_date_edit_get_time (E_DATE_EDIT (priv->delay_until));

	sopts->tracking_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->create_sent));
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->delivered)))
		sopts->track_when = E_DELIVERED;
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->delivered_opened)))
		sopts->track_when = E_DELIVERED_OPENED;
	else
		sopts->track_when = E_ALL;
	
	sopts->opened = gtk_combo_box_get_active ((GtkComboBox *) priv->when_opened);
	sopts->accepted = gtk_combo_box_get_active ((GtkComboBox *) priv->when_accepted);
	sopts->declined = gtk_combo_box_get_active ((GtkComboBox *) priv->when_declined);	
	sopts->completed = gtk_combo_box_get_active ((GtkComboBox *) priv->when_completed);
}

static void
e_send_options_fill_widgets_with_data (ESendOptionsDialog *sod)
{
	ESendOptionsDialogPrivate *priv;
	ESendOptionsGeneral *gopts;
	ESendOptionsStatusTracking *sopts;	
	time_t tmp;
	
	priv = sod->priv;
	gopts = sod->data->gopts;
	sopts = sod->data->sopts;	
	tmp = time (NULL);

	gtk_combo_box_set_active ((GtkComboBox *) priv->priority, gopts->priority);
	
	if (gopts->reply_enabled) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->reply_request), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->reply_request), FALSE);
	
	if (gopts->reply_convenient)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->reply_convenient), TRUE);
	else 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->reply_within), TRUE);
		
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->within_days), (gdouble) gopts->reply_within);
		
	if (gopts->expiration_enabled)	
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->expiration), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->expiration), FALSE);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->expire_after), (gdouble) gopts->expire_after);

	if (gopts->delay_enabled) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delay_delivery), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delay_delivery), FALSE);
	
	if (!gopts->delay_until || (!difftime (gopts->delay_until, tmp) < 0))
		e_date_edit_set_time (E_DATE_EDIT (priv->delay_until), 0);
	else
		e_date_edit_set_time (E_DATE_EDIT (priv->delay_until), gopts->delay_until);
	
	if (sopts->tracking_enabled) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->create_sent), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->create_sent), FALSE);
	
	switch (sopts->track_when) {
		case E_DELIVERED:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delivered), TRUE);
			break;
		case E_DELIVERED_OPENED:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delivered_opened), TRUE);
			break;
		case E_ALL:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_info), TRUE);
	}
	
	gtk_combo_box_set_active ((GtkComboBox *) priv->when_opened, sopts->opened);
	gtk_combo_box_set_active ((GtkComboBox *) priv->when_declined, sopts->declined);
	gtk_combo_box_set_active ((GtkComboBox *) priv->when_accepted, sopts->accepted);
	gtk_combo_box_set_active ((GtkComboBox *) priv->when_completed, sopts->completed);
		
}
	
static void 
sensitize_widgets (ESendOptionsDialog *sod)
{
	ESendOptionsDialogPrivate *priv;
	ESendOptionsGeneral *gopts;
	ESendOptionsStatusTracking *sopts;	

	priv = sod->priv;
	gopts = sod->data->gopts;
	sopts = sod->data->sopts;

	if (!gopts->reply_enabled) {
		gtk_widget_set_sensitive (priv->reply_convenient, FALSE);
		gtk_widget_set_sensitive (priv->reply_within, FALSE);
		gtk_widget_set_sensitive (priv->within_days, FALSE);
	}

	if (!gopts->expiration_enabled)
		gtk_widget_set_sensitive (priv->expire_after, FALSE);

	if (!gopts->delay_enabled){
		gtk_widget_set_sensitive (priv->delay_until, FALSE);
	}

	if (!sopts->tracking_enabled) {
		gtk_widget_set_sensitive (priv->delivered, FALSE);
		gtk_widget_set_sensitive (priv->delivered_opened, FALSE);
		gtk_widget_set_sensitive (priv->all_info, FALSE);
	}
}

static void
expiration_toggled_cb (GtkWidget *toggle, gpointer data)
{
	gboolean active;
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;

	sod = data;
	priv = sod->priv;

	active = GTK_TOGGLE_BUTTON (toggle)->active;

	gtk_widget_set_sensitive (priv->expire_after, active);
}

static void
reply_request_toggled_cb (GtkWidget *toggle, gpointer data)
{
	gboolean active;
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;
	
	sod = data;
	priv = sod->priv;
	active = GTK_TOGGLE_BUTTON (toggle)->active;

	gtk_widget_set_sensitive (priv->reply_convenient, active);
	gtk_widget_set_sensitive (priv->reply_within, active);
	gtk_widget_set_sensitive (priv->within_days, active);
	
}

static void
delay_delivery_toggled_cb (GtkWidget *toggle, gpointer data)
{
	gboolean active;
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;
	
	sod = data;
	priv = sod->priv;
	active = GTK_TOGGLE_BUTTON (toggle)->active;

	gtk_widget_set_sensitive (priv->delay_until, active);
}

static void
sent_item_toggled_cb (GtkWidget *toggle, gpointer data)
{
	gboolean active;
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;

	sod = data;
	priv = sod->priv;
	active = GTK_TOGGLE_BUTTON (toggle)->active;

	gtk_widget_set_sensitive (priv->delivered, active);
	gtk_widget_set_sensitive (priv->delivered_opened, active);
	gtk_widget_set_sensitive (priv->all_info, active);

}

static void
delay_until_date_changed_cb (GtkWidget *dedit, gpointer data)
{
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;
	ESendOptionsGeneral *gopts;
	time_t tmp, current;

	sod = data;
	priv = sod->priv;
	gopts = sod->data->gopts;	
	
	current = time (NULL);
	tmp = e_date_edit_get_time (E_DATE_EDIT (priv->delay_until));

	if ((difftime (tmp, current) < 0) || !e_date_edit_time_is_valid (E_DATE_EDIT (priv->delay_until)) 
					  || !e_date_edit_date_is_valid (E_DATE_EDIT (priv->delay_until)))
		e_date_edit_set_time (E_DATE_EDIT (priv->delay_until), 0);
	
}

static void
init_widgets (ESendOptionsDialog *sod)
{
	ESendOptionsDialogPrivate *priv;
	
	priv = sod->priv;	
	
	g_signal_connect (priv->expiration, "toggled", G_CALLBACK (expiration_toggled_cb), sod);
	g_signal_connect (priv->reply_request, "toggled", G_CALLBACK (reply_request_toggled_cb), sod);
	g_signal_connect (priv->delay_delivery, "toggled", G_CALLBACK (delay_delivery_toggled_cb), sod);	
	g_signal_connect (priv->create_sent, "toggled", G_CALLBACK (sent_item_toggled_cb), sod);

	g_signal_connect (priv->delay_until, "changed", G_CALLBACK (delay_until_date_changed_cb), sod);

}
	
static gboolean
get_widgets (ESendOptionsDialog *sod)
{
	ESendOptionsDialogPrivate *priv;

	priv = sod->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("send-options-dialog");
	if (!priv->main)
		return FALSE;

	priv->priority = GW ("combo-priority");
	priv->classification = GW ("classification-combo");
	priv->notebook = GW ("notebook");
	priv->reply_request = GW ("reply-request-button");
	priv->reply_convenient = GW ("reply-convinient");
	priv->reply_within = GW ("reply-within");
	priv->within_days = GW ("within-days");
	priv->delay_delivery = GW ("delay-delivery-button");
	priv->delay_until = GW ("until-date");
	gtk_widget_show (priv->delay_until);
	priv->expiration = GW ("expiration-button");
	priv->expire_after = GW ("expire-after");
	priv->create_sent = GW ("create-sent-button");
	priv->delivered = GW ("delivered");
	priv->delivered_opened = GW ("delivered-opened");
	priv->all_info = GW ("all-info");
	priv->autodelete = GW ("autodelete");
	priv->when_opened = GW ("open-combo");
	priv->when_declined = GW ("delete-combo");
	priv->when_accepted = GW ("accept-combo");
	priv->when_completed = GW ("complete-combo");
	priv->classification_label = GW ("classification-label");
	priv->gopts_label = GW ("gopts-label");
	priv->sopts_label = GW ("sopts-label");
	priv->priority_label = GW ("priority-label");
	priv->until_label = GW ("until-label");
	priv->opened_label = GW ("opened-label");
	priv->declined_label = GW ("declined-label");
	priv->accepted_label = GW ("accepted-label");
	priv->completed_label = GW ("completed-label");
	
#undef GW

	return (priv->priority
		&& priv->classification
		&& priv->reply_request
		&& priv->reply_convenient
		&& priv->reply_within
		&& priv->within_days
		&& priv->delay_delivery
		&& priv->delay_until
		&& priv->expiration
		&& priv->expire_after
		&& priv->create_sent
		&& priv->delivered
		&& priv->delivered_opened
		&& priv->autodelete
		&& priv->all_info
		&& priv->when_opened
		&& priv->when_declined
		&& priv->when_accepted
		&& priv->when_completed
		&& priv->classification_label
		&& priv->priority_label
		&& priv->opened_label
		&& priv->gopts_label
		&& priv->sopts_label
		&& priv->declined_label
		&& priv->accepted_label
		&& priv->completed_label);
	
}
	
static void
setup_widgets (ESendOptionsDialog *sod, Item_type type)
{
	ESendOptionsDialogPrivate *priv;

	priv = sod->priv;

	if (!priv->gopts_needed) {
		gtk_notebook_set_current_page (priv->notebook, 1);	
		gtk_widget_hide (priv->delay_until);
	} else
		gtk_notebook_set_show_tabs (priv->notebook, TRUE);

	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->priority_label), priv->priority);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->classification_label), priv->classification);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->accepted_label), priv->when_accepted);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->declined_label), priv->when_declined);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->opened_label), priv->when_opened);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->completed_label), priv->when_completed);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->until_label), priv->delay_until);

	switch (type) {
		case E_ITEM_MAIL:
			gtk_widget_hide (priv->accepted_label);
			gtk_widget_hide (priv->when_accepted);
			gtk_widget_hide (priv->completed_label);
			gtk_widget_hide (priv->when_completed);
			gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->declined_label), (_("When de_leted:")));
			break;
		case E_ITEM_CALENDAR:
			gtk_widget_hide (priv->completed_label);
			gtk_widget_hide (priv->when_completed);
		case E_ITEM_TASK:
			gtk_widget_hide (priv->classification_label);
			gtk_widget_hide (priv->classification);
			gtk_widget_set_sensitive (priv->autodelete, FALSE);
			break;
		default:
			break;
	}
}

ESendOptionsDialog *
e_sendoptions_dialog_new (void) {
	ESendOptionsDialog *sod;

	sod = g_object_new (E_TYPE_SENDOPTIONS_DIALOG, NULL);
	
	return sod;
}

void 
e_sendoptions_set_need_general_options (ESendOptionsDialog *sod, gboolean needed)
{
	g_return_if_fail (E_IS_SENDOPTIONS_DIALOG (sod));

	sod->priv->gopts_needed = needed;
}

gboolean
e_sendoptions_get_need_general_options (ESendOptionsDialog *sod)
{
	g_return_val_if_fail (E_IS_SENDOPTIONS_DIALOG (sod), FALSE);

	return sod->priv->gopts_needed;
}

GtkWidget * send_options_make_dateedit (void);

GtkWidget *
send_options_make_dateedit (void)
{
	EDateEdit *dedit;
	
	dedit = E_DATE_EDIT (e_date_edit_new ());

	e_date_edit_set_show_date (dedit, TRUE);
	e_date_edit_set_show_time (dedit, TRUE);

	return GTK_WIDGET (dedit);
}

gboolean 
e_sendoptions_dialog_run (ESendOptionsDialog *sod, GtkWidget *parent, Item_type type)
{
	ESendOptionsDialogPrivate *priv;
	GtkWidget *toplevel;
	int result;

	priv = sod->priv;
	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/e-send-options.glade", NULL, NULL);

	if (!priv->xml) {
		g_message ( G_STRLOC ": Could not load the Glade XML file ");
		return FALSE;
	}

	if (!get_widgets(sod)) {
		g_object_unref (priv->xml);
		g_message (G_STRLOC ": Could not get the Widgets \n");
		return FALSE;
	}
	
	setup_widgets (sod, type);

	toplevel =  gtk_widget_get_toplevel (priv->main);
	gtk_window_set_transient_for (GTK_WINDOW (toplevel),
				      GTK_WINDOW (parent));

	e_send_options_fill_widgets_with_data (sod);
	sensitize_widgets (sod);
	init_widgets (sod);

	result = gtk_dialog_run (GTK_DIALOG (priv->main));
	
	if (result == GTK_RESPONSE_OK) 
		e_send_options_get_widgets_data (sod);
	
	gtk_widget_hide (priv->main);
	gtk_widget_destroy (priv->main);
	g_object_unref (priv->xml);

	return TRUE;
}

static void
e_sendoptions_dialog_finalize (GObject *object)
{
}

static void
e_sendoptions_dialog_dispose (GObject *object)
{
}

/* Object initialization function for the task page */
static void
e_sendoptions_dialog_init (GObject *object)
{
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;
	ESendOptionsData *new;
	

	sod = E_SENDOPTIONS_DIALOG (object);
	new = g_new0 (ESendOptionsData, 1);
	new->gopts = g_new0 (ESendOptionsGeneral, 1);
	new->sopts = g_new0 (ESendOptionsStatusTracking, 1);
	priv = g_new0 (ESendOptionsDialogPrivate, 1);

	sod->priv = priv;
	sod->data = new;
	sod->data->initialized = FALSE;

	priv->gopts_needed = TRUE;
	priv->xml = NULL;

	priv->main = NULL;
	priv->notebook = NULL;
	priv->priority = NULL;
	priv->classification = NULL;
	priv->reply_request = NULL;
	priv->reply_convenient = NULL;
	priv->within_days = NULL;
	priv->delay_delivery = NULL;
	priv->delay_until = NULL;
	priv->expiration = NULL;
	priv->expire_after = NULL;
	priv->create_sent = NULL;
	priv->delivered =NULL;
	priv->delivered_opened = NULL;
	priv->all_info = NULL;
	priv->when_opened = NULL;
	priv->when_declined = NULL;
	priv->when_accepted = NULL;
	priv->when_completed = NULL;
	priv->classification_label = NULL;
	priv->priority_label = NULL;
	priv->opened_label = NULL;
	priv->gopts_label = NULL;
	priv->sopts_label = NULL;
	priv-> declined_label = NULL;
	priv->accepted_label = NULL;
	priv->completed_label = NULL;

}

/* Class initialization function for the Send Options */
static void
e_sendoptions_dialog_class_init (GObjectClass *object)
{
	ESendOptionsDialogClass *klass;
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	klass = E_SENDOPTIONS_DIALOG_CLASS (object);
	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = e_sendoptions_dialog_finalize;
	object_class->dispose = e_sendoptions_dialog_dispose;
}

GType e_sendoptions_dialog_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (ESendOptionsDialogClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc) e_sendoptions_dialog_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (ESendOptionsDialog),
     0,      /* n_preallocs */
     (GInstanceInitFunc) e_sendoptions_dialog_init,
 	NULL    /* instance_init */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "ESendOptionsDialogType",
                                   &info, 0);
  }
  return type;
}

