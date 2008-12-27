/*
 * e-mail-browser.c
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

#include "e-mail-browser.h"

#include <glib/gi18n.h>
#include <camel/camel-folder.h>

#include "mail/e-mail-reader.h"
#include "mail/e-mail-shell-module.h"
#include "mail/em-folder-tree-model.h"
#include "mail/em-format-html-display.h"
#include "mail/message-list.h"

#define E_MAIL_BROWSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_BROWSER, EMailBrowserPrivate))

struct _EMailBrowserPrivate {
	GtkUIManager *ui_manager;
	EShellModule *shell_module;
	GtkActionGroup *action_group;
};

enum {
	PROP_0,
	PROP_SHELL_MODULE,
	PROP_UI_MANAGER
};

static gpointer parent_class;

static void
mail_browser_set_shell_module (EMailBrowser *browser,
                               EShellModule *shell_module)
{
	g_return_if_fail (browser->priv->shell_module == NULL);

	browser->priv->shell_module = g_object_ref (shell_module);
}

static void
mail_browser_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_MODULE:
			mail_browser_set_shell_module (
				E_MAIL_BROWSER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_browser_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_MODULE:
			g_value_set_object (
				value, e_mail_browser_get_shell_module (
				E_MAIL_BROWSER (object)));
			return;

		case PROP_UI_MANAGER:
			g_value_set_object (
				value, e_mail_browser_get_ui_manager (
				E_MAIL_BROWSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_browser_dispose (GObject *object)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (object);

	if (priv->ui_manager != NULL) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	if (priv->shell_module != NULL) {
		g_object_unref (priv->shell_module);
		priv->shell_module = NULL;
	}

	if (priv->action_group != NULL) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_browser_constructed (GObject *object)
{
}

static GtkActionGroup *
mail_browser_get_action_group (EMailReader *reader)
{
	EMailBrowserPrivate *priv;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);

	return priv->action_group;
}

static EMFormatHTMLDisplay *
mail_browser_get_display (EMailReader *reader)
{
}

static CamelFolder *
mail_browser_get_folder (EMailReader *reader)
{
}

static const gchar *
mail_browser_get_folder_uri (EMailReader *reader)
{
}

static gboolean
mail_browser_get_hide_deleted (EMailReader *reader)
{
}

static MessageList *
mail_browser_get_message_list (EMailReader *reader)
{
}

static EMFolderTreeModel *
mail_browser_get_tree_model (EMailReader *reader)
{
	EMailBrowserPrivate *priv;
	EShellModule *shell_module;

	priv = E_MAIL_BROWSER_GET_PRIVATE (reader);
	shell_module = priv->shell_module;

	return e_mail_shell_module_get_folder_tree_model (shell_module);
}

static GtkWindow *
mail_browser_get_window (EMailReader *reader)
{
	return GTK_WINDOW (reader);
}

static void
mail_browser_class_init (EMailBrowserClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailBrowserPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_browser_set_property;
	object_class->get_property = mail_browser_get_property;
	object_class->dispose = mail_browser_dispose;
	object_class->constructed = mail_browser_constructed;

	g_object_class_install_property (
		object_class,
		PROP_SHELL_MODULE,
		g_param_spec_object (
			"shell-module",
			_("Shell Module"),
			_("The mail shell module"),
			E_TYPE_SHELL_MODULE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
mail_browser_iface_init (EMailReaderIface *iface)
{
	iface->get_action_group = mail_browser_get_action_group;
	iface->get_display = mail_browser_get_display;
	iface->get_folder = mail_browser_get_folder;
	iface->get_folder_uri = mail_browser_get_folder_uri;
	iface->get_hide_deleted = mail_browser_get_hide_deleted;
	iface->get_message_list = mail_browser_get_message_list;
	iface->get_tree_model = mail_browser_get_tree_model;
	iface->get_window = mail_browser_get_window;
}

static void
mail_browser_init (EMailBrowser *browser)
{
	browser->priv = E_MAIL_BROWSER_GET_PRIVATE (browser);

	browser->priv->ui_manager = gtk_ui_manager_new ();
	browser->priv->action_group = gtk_action_group_new ("mail-browser");
}

GType
e_mail_browser_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailBrowserClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_browser_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailBrowser),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_browser_init,
			NULL   /* value_table */
		};

		static const GInterfaceInfo iface_info = {
			(GInterfaceInitFunc) mail_browser_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL  /* interface_data */
		};

		type = g_type_register_static (
			GTK_TYPE_WINDOW, "EMailBrowser", &type_info, 0);

		g_type_add_interface_static (
			type, E_TYPE_MAIL_BROWSER, &iface_info);
	}

	return type;
}

GtkWidget *
e_mail_browser_new (EShellModule *shell_module)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);

	return g_object_new (
		E_TYPE_MAIL_BROWSER,
		"shell-module", shell_module, NULL);
}

EShellModule *
e_mail_browser_get_shell_module (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), NULL);

	return browser->priv->shell_module;
}

GtkUIManager *
e_mail_browser_get_ui_manager (EMailBrowser *browser)
{
	g_return_val_if_fail (E_IS_MAIL_BROWSER (browser), NULL);

	return browser->priv->ui_manager;
}
