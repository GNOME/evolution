/*
 * e-mail-junk-hook.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-junk-hook.h"

#include <glib/gi18n.h>

#include "e-util/e-alert-dialog.h"
#include "shell/e-shell.h"

#include "mail/em-junk.h"
#include "mail/em-utils.h"
#include "mail/mail-session.h"

#define E_MAIL_JUNK_HOOK_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_JUNK_HOOK, EMailJunkHookPrivate))

struct _EMailJunkHookPrivate {
	EMJunkInterface interface;
};

struct ErrorData {
	const gchar *error_message;
	GError *error;
};

static gpointer parent_class;
static GType mail_junk_hook_type;

static gboolean
mail_junk_hook_idle_cb (struct ErrorData *data)
{
	GtkWidget *widget;

	widget = e_alert_dialog_new_for_args (e_shell_get_active_window (NULL),
		data->error_message, data->error->message, NULL);
	em_utils_show_error_silent (widget);

	g_error_free (data->error);
	g_slice_free (struct ErrorData, data);

	return FALSE;
}

static void
mail_junk_hook_error (const gchar *error_message,
                      GError *error)
{
	struct ErrorData *data;

	g_return_if_fail (error != NULL);

	data = g_slice_new (struct ErrorData);
	data->error_message = error_message;
	data->error = error;

	g_idle_add ((GSourceFunc) mail_junk_hook_idle_cb, data);
}

static const gchar *
mail_junk_hook_get_name (CamelJunkPlugin *junk_plugin)
{
	EMJunkInterface *interface;

	interface = (EMJunkInterface *) junk_plugin;

	if (!interface->hook->plugin->enabled) {
		/* Translators: "None" for a junk hook name,
		 * when the junk plugin is not enabled. */
		return C_("mail-junk-hook", "None");
	}

	return interface->hook->plugin->name;
}

static void
mail_junk_hook_plugin_init (CamelJunkPlugin *junk_plugin)
{
	EMJunkInterface *interface;
	EPluginClass *class;

	interface = (EMJunkInterface *) junk_plugin;

	class = E_PLUGIN_GET_CLASS (interface->hook->plugin);
	g_return_if_fail (class->enable != NULL);

	class->enable (interface->hook->plugin, 1);
}

static gboolean
mail_junk_hook_check_junk (CamelJunkPlugin *junk_plugin,
                           CamelMimeMessage *mime_message)
{
	EMJunkTarget target = { mime_message, NULL };
	EMJunkInterface *interface;
	gpointer result;

	interface = (EMJunkInterface *) junk_plugin;

	if (!interface->hook->plugin->enabled)
		return FALSE;

	result = e_plugin_invoke (
		interface->hook->plugin,
		interface->check_junk, &target);

	if (target.error != NULL)
		mail_junk_hook_error ("mail:junk-check-error", target.error);

	return (result != NULL);
}

static void
mail_junk_hook_report_junk (CamelJunkPlugin *junk_plugin,
                            CamelMimeMessage *mime_message)
{
	EMJunkTarget target = { mime_message, NULL };
	EMJunkInterface *interface;

	interface = (EMJunkInterface *) junk_plugin;

	if (!interface->hook->plugin->enabled)
		return;

	e_plugin_invoke (
		interface->hook->plugin,
		interface->report_junk, &target);

	if (target.error != NULL)
		mail_junk_hook_error ("mail:junk-report-error", target.error);
}

static void
mail_junk_hook_report_notjunk (CamelJunkPlugin *junk_plugin,
                               CamelMimeMessage *mime_message)
{
	EMJunkTarget target = { mime_message, NULL };
	EMJunkInterface *interface;

	interface = (EMJunkInterface *) junk_plugin;

	if (!interface->hook->plugin->enabled)
		return;

	e_plugin_invoke (
		interface->hook->plugin,
		interface->report_notjunk, &target);

	if (target.error != NULL)
		mail_junk_hook_error (
			"mail:junk-not-report-error", target.error);
}

static void
mail_junk_hook_commit_reports (CamelJunkPlugin *junk_plugin)
{
	EMJunkInterface *interface;

	interface = (EMJunkInterface *) junk_plugin;

	if (!interface->hook->plugin->enabled)
		return;

	e_plugin_invoke (
		interface->hook->plugin,
		interface->commit_reports, NULL);
}

static void
mail_junk_hook_finalize (GObject *object)
{
	EMailJunkHookPrivate *priv;

	priv = E_MAIL_JUNK_HOOK_GET_PRIVATE (object);

	g_free (priv->interface.check_junk);
	g_free (priv->interface.report_junk);
	g_free (priv->interface.report_notjunk);
	g_free (priv->interface.commit_reports);
	g_free (priv->interface.validate_binary);
	g_free (priv->interface.plugin_name);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
mail_junk_hook_construct (EPluginHook *hook,
                          EPlugin *plugin,
                          xmlNodePtr node)
{
	EMailJunkHookPrivate *priv;
	gchar *property;

	priv = E_MAIL_JUNK_HOOK_GET_PRIVATE (hook);

	/* Chain up to parent's construct() method. */
	if (E_PLUGIN_HOOK_CLASS (parent_class)->construct (hook, plugin, node) == -1)
		return -1;

	if (!plugin->enabled)
		return -1;

	node = xmlFirstElementChild (node);

	if (node == NULL)
		return -1;

	if (g_strcmp0 ((gchar *) node->name, "interface") != 0)
		return -1;

	property = e_plugin_xml_prop (node, "check_junk");
	priv->interface.check_junk = property;

	property = e_plugin_xml_prop (node, "report_junk");
	priv->interface.report_junk = property;

	property = e_plugin_xml_prop (node, "report_non_junk");
	priv->interface.report_notjunk = property;

	property = e_plugin_xml_prop (node, "commit_reports");
	priv->interface.commit_reports = property;

	property = e_plugin_xml_prop (node, "validate_binary");
	priv->interface.validate_binary = property;

	property = e_plugin_xml_prop (node, "name");
	priv->interface.plugin_name = property;

	if (priv->interface.check_junk == NULL)
		return -1;

	if (priv->interface.report_junk == NULL)
		return -1;

	if (priv->interface.report_notjunk == NULL)
		return -1;

	if (priv->interface.commit_reports == NULL)
		return -1;

	mail_session_add_junk_plugin (
		priv->interface.plugin_name, &priv->interface.camel);

	return 0;
}

static void
mail_junk_hook_class_init (EMailJunkHookClass *class)
{
	GObjectClass *object_class;
	EPluginHookClass *plugin_hook_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailJunkHookPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = mail_junk_hook_finalize;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->construct = mail_junk_hook_construct;
	plugin_hook_class->id = "org.gnome.evolution.mail.junk:1.0";
}

static void
mail_junk_hook_init (EMailJunkHook *mail_junk_hook)
{
	EMJunkInterface *interface;

	mail_junk_hook->priv = E_MAIL_JUNK_HOOK_GET_PRIVATE (mail_junk_hook);

	interface = &mail_junk_hook->priv->interface;
	interface->camel.get_name = mail_junk_hook_get_name;
	interface->camel.api_version = 1;
	interface->camel.check_junk = mail_junk_hook_check_junk;
	interface->camel.report_junk = mail_junk_hook_report_junk;
	interface->camel.report_notjunk = mail_junk_hook_report_notjunk;
	interface->camel.commit_reports = mail_junk_hook_commit_reports;
	interface->camel.init = mail_junk_hook_plugin_init;
	interface->hook = E_PLUGIN_HOOK (mail_junk_hook);
}

GType
e_mail_junk_hook_get_type (void)
{
	return mail_junk_hook_type;
}

void
e_mail_junk_hook_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (EMailJunkHookClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_junk_hook_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMailJunkHook),
		0,     /* n_preallocs */
		(GInstanceInitFunc) mail_junk_hook_init,
		NULL   /* value_table */
	};

	mail_junk_hook_type = g_type_module_register_type (
		type_module, E_TYPE_PLUGIN_HOOK,
		"EMailJunkHook", &type_info, 0);
}
