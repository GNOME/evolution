/*
 * e-mail-config-sendmail-backend.c
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

#include <camel/camel.h>
#include <e-util/e-util.h>
#include <libebackend/libebackend.h>

#include "e-mail-config-sendmail-backend.h"

struct _EMailConfigSendmailBackendPrivate
{
	GtkWidget *custom_binary_entry; /* not referenced */
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailConfigSendmailBackend, e_mail_config_sendmail_backend, E_TYPE_MAIL_CONFIG_SERVICE_BACKEND, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailConfigSendmailBackend))

static void
mail_config_sendmail_backend_insert_widgets (EMailConfigServiceBackend *backend,
                                             GtkBox *parent)
{
	EMailConfigSendmailBackend *sendmail_backend;
	CamelSettings *settings;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *use_custom_binary_check;
	GtkWidget *custom_binary_entry;
	GtkWidget *use_custom_args_check;
	GtkWidget *custom_args_entry;
	GtkWidget *send_in_offline;
	gchar *markup;
	PangoAttribute *attr;
	PangoAttrList *attr_list;

	sendmail_backend = E_MAIL_CONFIG_SENDMAIL_BACKEND (backend);
	settings = e_mail_config_service_backend_get_settings (backend);

	markup = g_markup_printf_escaped ("<b>%s</b>", _("Configuration"));
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_grid_new ();
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);

	container = widget;

	widget = gtk_check_button_new_with_mnemonic (_("_Use custom binary, instead of “sendmail”"));
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	use_custom_binary_check = widget;

	widget = gtk_label_new_with_mnemonic (_("_Custom binary:"));
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	custom_binary_entry = widget;

	sendmail_backend->priv->custom_binary_entry = widget;

	e_binding_bind_property (
		use_custom_binary_check, "active",
		label, "sensitive",
		G_BINDING_SYNC_CREATE);

	widget = gtk_check_button_new_with_mnemonic (_("U_se custom arguments"));
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 2, 1);
	use_custom_args_check = widget;

	widget = gtk_label_new_with_mnemonic (_("Cus_tom arguments:"));
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 1, 1);
	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 3, 1, 1);
	custom_args_entry = widget;

	e_binding_bind_property (
		use_custom_args_check, "active",
		label, "sensitive",
		G_BINDING_SYNC_CREATE);

	widget = gtk_label_new (_(
		"Default arguments are “-i -f %F -- %R”, where\n"
		"   %F — stands for the From address\n"
		"   %R — stands for the recipient addresses"));
	gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_yalign (GTK_LABEL (widget), 0);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 4, 1, 1);

	attr_list = pango_attr_list_new ();
	attr = pango_attr_style_new (PANGO_STYLE_ITALIC);
	pango_attr_list_insert (attr_list, attr);
	gtk_label_set_attributes (GTK_LABEL (widget), attr_list);
	pango_attr_list_unref (attr_list);

	widget = gtk_check_button_new_with_mnemonic (_("Send mail also when in offline _mode"));
	gtk_grid_attach (GTK_GRID (container), widget, 0, 5, 2, 1);
	send_in_offline = widget;

	e_binding_bind_property (
		use_custom_binary_check, "active",
		custom_binary_entry, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		settings, "use-custom-binary",
		use_custom_binary_check, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_object_text_property (
		settings, "custom-binary",
		custom_binary_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		use_custom_args_check, "active",
		custom_args_entry, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		settings, "use-custom-args",
		use_custom_args_check, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_object_text_property (
		settings, "custom-args",
		custom_args_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		settings, "send-in-offline",
		send_in_offline, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	gtk_widget_show_all (container);
}

static gboolean
mail_config_sendmail_backend_check_complete (EMailConfigServiceBackend *backend)
{
	EMailConfigSendmailBackend *sendmail_backend;
	CamelSettings *settings;
	gboolean use_custom_binary = FALSE;
	gchar *custom_binary = NULL;
	gboolean res = TRUE;

	sendmail_backend = E_MAIL_CONFIG_SENDMAIL_BACKEND (backend);
	settings = e_mail_config_service_backend_get_settings (backend);

	g_object_get (
		G_OBJECT (settings),
		"use-custom-binary", &use_custom_binary,
		"custom-binary", &custom_binary,
		NULL);

	if (custom_binary)
		g_strstrip (custom_binary);

	if (use_custom_binary && (!custom_binary || !*custom_binary))
		res = FALSE;

	g_free (custom_binary);

	e_util_set_entry_issue_hint (sendmail_backend->priv->custom_binary_entry, res ? NULL : _("Custom binary cannot be empty"));

	return res;
}

static void
e_mail_config_sendmail_backend_class_init (EMailConfigSendmailBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "sendmail";
	backend_class->insert_widgets = mail_config_sendmail_backend_insert_widgets;
	backend_class->check_complete = mail_config_sendmail_backend_check_complete;
}

static void
e_mail_config_sendmail_backend_class_finalize (EMailConfigSendmailBackendClass *class)
{
}

static void
e_mail_config_sendmail_backend_init (EMailConfigSendmailBackend *backend)
{
	backend->priv = e_mail_config_sendmail_backend_get_instance_private (backend);
}

void
e_mail_config_sendmail_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_sendmail_backend_register_type (type_module);
}

