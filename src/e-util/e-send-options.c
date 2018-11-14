/*
 * Evolution calendar - Main page of the Groupwise send options Dialog
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
 *		Chenthill Palanisamy <pchenthill@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-send-options.h"

#include <string.h>
#include <glib/gi18n.h>
#include <time.h>

#include "e-dateedit.h"
#include "e-misc-utils.h"
#include "e-util-private.h"

#define E_SEND_OPTIONS_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SEND_OPTIONS_DIALOG, ESendOptionsDialogPrivate))

struct _ESendOptionsDialogPrivate {
	GtkBuilder *builder;

	gboolean gopts_needed;
	gboolean global;

	/* Widgets */

	GtkWidget *main;

	/* Noteboook to add options page and status tracking page*/
	GtkNotebook *notebook;

	GtkWidget *status;

	/* priority */
	GtkWidget *priority;

	/* Security */
	GtkWidget *security;

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
	GtkWidget *security_label;
	GtkWidget *priority_label;
	GtkWidget *gopts_label;
	GtkWidget *opened_label;
	GtkWidget *declined_label;
	GtkWidget *accepted_label;
	GtkWidget *completed_label;
	GtkWidget *until_label;
        gchar *help_section;
};

static void e_send_options_dialog_finalize (GObject *object);
static void e_send_options_cb (GtkDialog *dialog, gint state, gpointer func_data);

enum {
	SOD_RESPONSE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (
	ESendOptionsDialog,
	e_send_options_dialog,
	G_TYPE_OBJECT)

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
	gopts->security = gtk_combo_box_get_active ((GtkComboBox *) priv->security);

	gopts->reply_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->reply_request));
	gopts->reply_convenient = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->reply_convenient));
	gopts->reply_within = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->within_days));

	gopts->expiration_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->expiration));
	gopts->expire_after = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->expire_after));
	gopts->delay_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->delay_delivery));

	if (e_date_edit_date_is_valid (E_DATE_EDIT (priv->delay_until)) &&
								e_date_edit_time_is_valid (E_DATE_EDIT (priv->delay_until)))
		gopts->delay_until = e_date_edit_get_time (E_DATE_EDIT (priv->delay_until));

	sopts->tracking_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->create_sent));

	sopts->autodelete = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->autodelete));

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
	gtk_combo_box_set_active ((GtkComboBox *) priv->security, gopts->security);

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

	if (!gopts->delay_until || difftime (gopts->delay_until, tmp) < 0)
		e_date_edit_set_time (E_DATE_EDIT (priv->delay_until), 0);
	else
		e_date_edit_set_time (E_DATE_EDIT (priv->delay_until), gopts->delay_until);

	if (sopts->tracking_enabled)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->create_sent), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->create_sent), FALSE);

	if (sopts->autodelete)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->autodelete), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->autodelete), FALSE);

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

	if (!gopts->delay_enabled) {
		gtk_widget_set_sensitive (priv->delay_until, FALSE);
	}

	if (!sopts->tracking_enabled) {
		gtk_widget_set_sensitive (priv->delivered, FALSE);
		gtk_widget_set_sensitive (priv->delivered_opened, FALSE);
		gtk_widget_set_sensitive (priv->all_info, FALSE);
		gtk_widget_set_sensitive (priv->autodelete, FALSE);
	}
}

static void
expiration_toggled_cb (GtkToggleButton *toggle,
                       gpointer data)
{
	gboolean active;
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;

	sod = data;
	priv = sod->priv;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (priv->expire_after, active);
}

static void
reply_request_toggled_cb (GtkToggleButton *toggle,
                          gpointer data)
{
	gboolean active;
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;

	sod = data;
	priv = sod->priv;
	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (priv->reply_convenient, active);
	gtk_widget_set_sensitive (priv->reply_within, active);
	gtk_widget_set_sensitive (priv->within_days, active);

}

static void
delay_delivery_toggled_cb (GtkToggleButton *toggle,
                           gpointer data)
{
	gboolean active;
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;

	sod = data;
	priv = sod->priv;
	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (priv->delay_until, active);
}

static void
sent_item_toggled_cb (GtkToggleButton *toggle,
                      gpointer data)
{
	gboolean active;
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;

	sod = data;
	priv = sod->priv;
	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (priv->delivered, active);
	gtk_widget_set_sensitive (priv->delivered_opened, active);
	gtk_widget_set_sensitive (priv->all_info, active);
	gtk_widget_set_sensitive (priv->autodelete, active);
}

static void
delay_until_date_changed_cb (GtkWidget *dedit,
                             gpointer data)
{
	ESendOptionsDialog *sod;
	ESendOptionsDialogPrivate *priv;
	time_t tmp, current;

	sod = data;
	priv = sod->priv;

	current = time (NULL);
	tmp = e_date_edit_get_time (E_DATE_EDIT (priv->delay_until));

	if ((difftime (tmp, current) < 0) || !e_date_edit_time_is_valid (E_DATE_EDIT (priv->delay_until))
					  || !e_date_edit_date_is_valid (E_DATE_EDIT (priv->delay_until)))
		e_date_edit_set_time (E_DATE_EDIT (priv->delay_until), 0);

}
static void
page_changed_cb (GtkNotebook *notebook,
                 GtkWidget *page,
                 gint num,
                 gpointer data)
{
	ESendOptionsDialog *sod = data;
	ESendOptionsDialogPrivate *priv = sod->priv;

	e_send_options_get_widgets_data (sod);
	if (num > 0) {
		GtkWidget *child;

		if (num == 1) {
			gtk_widget_hide (priv->accepted_label);
			gtk_widget_hide (priv->when_accepted);
			gtk_widget_hide (priv->completed_label);
			gtk_widget_hide (priv->when_completed);
			gtk_widget_set_sensitive (priv->autodelete, TRUE);
			sod->data->sopts = sod->data->mopts;

			child = gtk_notebook_get_nth_page (notebook, 1);
			if (child != priv->status && (!GTK_IS_BIN (child) || gtk_bin_get_child (GTK_BIN (child)) != priv->status))
				gtk_widget_reparent (priv->status, child);
		} else if (num == 2) {
			gtk_widget_hide (priv->completed_label);
			gtk_widget_hide (priv->when_completed);
			gtk_widget_set_sensitive (priv->autodelete, FALSE);

			gtk_widget_show (priv->accepted_label);
			gtk_widget_show (priv->when_accepted);
			sod->data->sopts = sod->data->copts;

			child = gtk_notebook_get_nth_page (notebook, 2);
			if (gtk_bin_get_child (GTK_BIN (child)) != priv->status)
				gtk_widget_reparent (priv->status, child);
		} else {
			gtk_widget_set_sensitive (priv->autodelete, FALSE);

			gtk_widget_show (priv->completed_label);
			gtk_widget_show (priv->when_completed);
			gtk_widget_show (priv->accepted_label);
			gtk_widget_show (priv->when_accepted);
			sod->data->sopts = sod->data->topts;

			child = gtk_notebook_get_nth_page (notebook, 3);
			if (gtk_bin_get_child (GTK_BIN (child)) != priv->status)
				gtk_widget_reparent (priv->status, child);
		}
	}
	e_send_options_fill_widgets_with_data (sod);
}

static void
init_widgets (ESendOptionsDialog *sod)
{
	ESendOptionsDialogPrivate *priv;

	priv = sod->priv;

	g_signal_connect (
		priv->expiration, "toggled",
		G_CALLBACK (expiration_toggled_cb), sod);
	g_signal_connect (
		priv->reply_request, "toggled",
		G_CALLBACK (reply_request_toggled_cb), sod);
	g_signal_connect (
		priv->delay_delivery, "toggled",
		G_CALLBACK (delay_delivery_toggled_cb), sod);
	g_signal_connect (
		priv->create_sent, "toggled",
		G_CALLBACK (sent_item_toggled_cb), sod);

	g_signal_connect (
		priv->main, "response",
		G_CALLBACK (e_send_options_cb), sod);
	g_signal_connect (
		priv->delay_until, "changed",
		G_CALLBACK (delay_until_date_changed_cb), sod);

	if (priv->global)
		g_signal_connect (
			priv->notebook, "switch-page",
			G_CALLBACK (page_changed_cb), sod);

}

static gboolean
get_widgets (ESendOptionsDialog *sod)
{
	ESendOptionsDialogPrivate *priv;
	GtkBuilder *builder;

	priv = sod->priv;
	builder = sod->priv->builder;

	priv->main = e_builder_get_widget (builder, "send-options-dialog");
	if (!priv->main)
		return FALSE;

	priv->priority = e_builder_get_widget (builder, "combo-priority");
	priv->status = e_builder_get_widget (builder, "status-tracking");
	priv->security = e_builder_get_widget (builder, "security-combo");
	priv->notebook = (GtkNotebook *) e_builder_get_widget (builder, "notebook");
	priv->reply_request = e_builder_get_widget (builder, "reply-request-button");
	priv->reply_convenient = e_builder_get_widget (builder, "reply-convinient");
	priv->reply_within = e_builder_get_widget (builder, "reply-within");
	priv->within_days = e_builder_get_widget (builder, "within-days");
	priv->delay_delivery = e_builder_get_widget (builder, "delay-delivery-button");
	priv->delay_until = e_builder_get_widget (builder, "until-date");
	gtk_widget_show (priv->delay_until);
	priv->expiration = e_builder_get_widget (builder, "expiration-button");
	priv->expire_after = e_builder_get_widget (builder, "expire-after");
	priv->create_sent = e_builder_get_widget (builder, "create-sent-button");
	priv->delivered = e_builder_get_widget (builder, "delivered");
	priv->delivered_opened = e_builder_get_widget (builder, "delivered-opened");
	priv->all_info = e_builder_get_widget (builder, "all-info");
	priv->autodelete = e_builder_get_widget (builder, "autodelete");
	priv->when_opened = e_builder_get_widget (builder, "open-combo");
	priv->when_declined = e_builder_get_widget (builder, "delete-combo");
	priv->when_accepted = e_builder_get_widget (builder, "accept-combo");
	priv->when_completed = e_builder_get_widget (builder, "complete-combo");
	priv->security_label = e_builder_get_widget (builder, "security-label");
	priv->gopts_label = e_builder_get_widget (builder, "gopts-label");
	priv->priority_label = e_builder_get_widget (builder, "priority-label");
	priv->until_label = e_builder_get_widget (builder, "until-label");
	priv->opened_label = e_builder_get_widget (builder, "opened-label");
	priv->declined_label = e_builder_get_widget (builder, "declined-label");
	priv->accepted_label = e_builder_get_widget (builder, "accepted-label");
	priv->completed_label = e_builder_get_widget (builder, "completed-label");

	return (priv->priority
		&& priv->security
		&& priv->status
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
		&& priv->security_label
		&& priv->priority_label
		&& priv->opened_label
		&& priv->gopts_label
		&& priv->declined_label
		&& priv->accepted_label
		&& priv->completed_label);

}

static void
setup_widgets (ESendOptionsDialog *sod,
               Item_type type)
{
	ESendOptionsDialogPrivate *priv;

	priv = sod->priv;

	gtk_notebook_set_show_border (priv->notebook, FALSE);
	if (!priv->gopts_needed) {
		gtk_notebook_set_show_tabs (priv->notebook, FALSE);
		gtk_notebook_set_current_page (priv->notebook, 1);
		gtk_widget_hide (priv->delay_until);
	} else
		gtk_notebook_set_show_tabs (priv->notebook, TRUE);

	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->priority_label), priv->priority);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->security_label), priv->security);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->accepted_label), priv->when_accepted);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->declined_label), priv->when_declined);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->opened_label), priv->when_opened);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->completed_label), priv->when_completed);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->until_label), priv->delay_until);

	if (priv->global) {
		GtkWidget *widget, *page;

		widget = gtk_label_new (_("Mail"));
		page = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
		gtk_widget_reparent (priv->status, page);
		gtk_notebook_append_page (priv->notebook, page, widget);
		gtk_container_child_set (GTK_CONTAINER (priv->notebook), page, "tab-fill", FALSE, "tab-expand", FALSE, NULL);
		gtk_widget_show (page);
		gtk_widget_show (widget);

		widget = gtk_label_new (_("Calendar"));
		page = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
		gtk_notebook_append_page (priv->notebook, page, widget);
		gtk_container_child_set (GTK_CONTAINER (priv->notebook), page, "tab-fill", FALSE, "tab-expand", FALSE, NULL);
		gtk_widget_show (page);
		gtk_widget_show (widget);

		widget = gtk_label_new (_("Task"));
		page = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
		gtk_notebook_append_page (priv->notebook, page, widget);
		gtk_container_child_set (GTK_CONTAINER (priv->notebook), page, "tab-fill", FALSE, "tab-expand", FALSE, NULL);
		gtk_widget_show (page);
		gtk_widget_show (widget);

		gtk_notebook_set_show_tabs (priv->notebook, TRUE);
	}

	switch (type) {
		case E_ITEM_MAIL:
			priv->help_section = g_strdup ("groupwise-placeholder");
			gtk_widget_hide (priv->accepted_label);
			gtk_widget_hide (priv->when_accepted);
			gtk_widget_hide (priv->completed_label);
			gtk_widget_hide (priv->when_completed);
			gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->declined_label), (_("When de_leted:")));
			break;
		case E_ITEM_CALENDAR:
			priv->help_section = g_strdup ("groupwise-placeholder");
			gtk_widget_hide (priv->completed_label);
			gtk_widget_hide (priv->when_completed);
			break;
		case E_ITEM_TASK:
			priv->help_section = g_strdup ("groupwise-placeholder");
			gtk_widget_hide (priv->security_label);
			gtk_widget_hide (priv->security);
			gtk_widget_set_sensitive (priv->autodelete, FALSE);
			break;
		default:
			break;
	}
}

ESendOptionsDialog *
e_send_options_dialog_new (void)
{
	ESendOptionsDialog *sod;

	sod = g_object_new (E_TYPE_SEND_OPTIONS_DIALOG, NULL);

	return sod;
}

void
e_send_options_set_need_general_options (ESendOptionsDialog *sod,
                                         gboolean needed)
{
	g_return_if_fail (E_IS_SEND_OPTIONS_DIALOG (sod));

	sod->priv->gopts_needed = needed;
}

gboolean
e_send_options_get_need_general_options (ESendOptionsDialog *sod)
{
	g_return_val_if_fail (E_IS_SEND_OPTIONS_DIALOG (sod), FALSE);

	return sod->priv->gopts_needed;
}

gboolean
e_send_options_set_global (ESendOptionsDialog *sod,
                           gboolean set)
{
	g_return_val_if_fail (E_IS_SEND_OPTIONS_DIALOG (sod), FALSE);

	sod->priv->global = set;

	return TRUE;
}

static void
e_send_options_cb (GtkDialog *dialog,
                   gint state,
                   gpointer func_data)
{
	ESendOptionsDialogPrivate *priv;
	ESendOptionsDialog *sod;

	sod = func_data;
	priv = sod->priv;

	switch (state) {
		case GTK_RESPONSE_OK:
			e_send_options_get_widgets_data (sod);
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

	g_signal_emit (func_data, signals[SOD_RESPONSE], 0, state);
}

gboolean
e_send_options_dialog_run (ESendOptionsDialog *sod,
                           GtkWidget *parent,
                           Item_type type)
{
	ESendOptionsDialogPrivate *priv;
	GtkWidget *toplevel;

	g_return_val_if_fail (sod != NULL || E_IS_SEND_OPTIONS_DIALOG (sod), FALSE);

	priv = sod->priv;

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_DATE_EDIT);

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "e-send-options.ui");

	if (!get_widgets (sod)) {
		g_object_unref (priv->builder);
		g_message (G_STRLOC ": Could not get the Widgets \n");
		return FALSE;
	}

	if (priv->global) {
		g_free (sod->data->sopts);
		sod->data->sopts = sod->data->mopts;
	}

	setup_widgets (sod, type);

	toplevel = gtk_widget_get_toplevel (priv->main);
	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (toplevel),
				      GTK_WINDOW (parent));

	e_send_options_fill_widgets_with_data (sod);
	sensitize_widgets (sod);
	init_widgets (sod);
	gtk_window_set_modal ((GtkWindow *) priv->main, TRUE);

	gtk_widget_show (priv->main);

	return TRUE;
}

static void
e_send_options_dialog_finalize (GObject *object)
{
	ESendOptionsDialog *sod;

	sod = E_SEND_OPTIONS_DIALOG (object);

	g_free (sod->priv->help_section);

	g_free (sod->data->gopts);

	if (!sod->priv->global)
		g_free (sod->data->sopts);

	g_free (sod->data->mopts);
	g_free (sod->data->copts);
	g_free (sod->data->topts);
	g_free (sod->data);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_send_options_dialog_parent_class)->finalize (object);
}

/* Object initialization function for the task page */
static void
e_send_options_dialog_init (ESendOptionsDialog *sod)
{
	ESendOptionsData *new;

	new = g_new0 (ESendOptionsData, 1);
	new->gopts = g_new0 (ESendOptionsGeneral, 1);
	new->sopts = g_new0 (ESendOptionsStatusTracking, 1);
	new->mopts = g_new0 (ESendOptionsStatusTracking, 1);
	new->copts = g_new0 (ESendOptionsStatusTracking, 1);
	new->topts = g_new0 (ESendOptionsStatusTracking, 1);

	sod->priv = E_SEND_OPTIONS_DIALOG_GET_PRIVATE (sod);

	sod->data = new;
	sod->data->initialized = FALSE;
	sod->data->gopts->security = 0;

	sod->priv->gopts_needed = TRUE;
}

/* Class initialization function for the Send Options */
static void
e_send_options_dialog_class_init (ESendOptionsDialogClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ESendOptionsDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_send_options_dialog_finalize;

	signals[SOD_RESPONSE] = g_signal_new (
		"sod_response",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ESendOptionsDialogClass, sod_response),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

}
