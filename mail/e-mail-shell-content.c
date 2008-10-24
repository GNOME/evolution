/*
 * e-mail-shell-content.c
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

#include "e-mail-shell-content.h"

#include <glib/gi18n.h>

#include "e-util/gconf-bridge.h"

#include "em-folder-browser.h"
#include "em-search-context.h"

#define E_MAIL_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_CONTENT, EMailShellContentPrivate))

struct _EMailShellContentPrivate {
	GtkWidget *paned;
	GtkWidget *msglist;
	GtkWidget *preview;

	guint paned_binding_id;

	guint preview_visible	: 1;
	guint vertical_view	: 1;
};

enum {
	PROP_0,
	PROP_PREVIEW_VISIBLE,
	PROP_VERTICAL_VIEW
};

static gpointer parent_class;

static void
mail_shell_content_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			e_mail_shell_content_set_preview_visible (
				E_MAIL_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_VERTICAL_VIEW:
			e_mail_shell_content_set_vertical_view (
				E_MAIL_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_content_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value,
				e_mail_shell_content_get_preview_visible (
				E_MAIL_SHELL_CONTENT (object)));
			return;

		case PROP_VERTICAL_VIEW:
			g_value_set_boolean (
				value,
				e_mail_shell_content_get_vertical_view (
				E_MAIL_SHELL_CONTENT (object)));
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_content_dispose (GObject *object)
{
	EMailShellContentPrivate *priv;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (object);

	if (priv->paned != NULL) {
		g_object_unref (priv->paned);
		priv->paned = NULL;
	}

	if (priv->msglist != NULL) {
		g_object_unref (priv->msglist);
		priv->msglist = NULL;
	}

	if (priv->preview != NULL) {
		g_object_unref (priv->preview);
		priv->preview = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_shell_content_finalize (GObject *object)
{
	EMailShellContentPrivate *priv;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
mail_shell_content_constructed (GObject *object)
{
	EMailShellContentPrivate *priv;
	GConfBridge *bridge;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *key;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = gtk_vpaned_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	/*widget = em_folder_browser_new ();*/
	widget = gtk_label_new ("Message List");
	gtk_paned_add1 (GTK_PANED (container), widget);
	priv->msglist = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_paned_add2 (GTK_PANED (container), widget);
	gtk_widget_show (widget);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (priv->paned);
	key = "/apps/evolution/mail/display/paned_size";
	gconf_bridge_bind_property_delayed (bridge, key, object, "position");
}

static guint32
mail_shell_content_check_state (EShellContent *shell_content)
{
	EMailShellContent *mail_shell_content;
	guint32 state = 0;

	mail_shell_content = E_MAIL_SHELL_CONTENT (shell_content);

	return state;
}

static void
mail_shell_content_class_init (EMailShellContentClass *class)
{
	GObjectClass *object_class;
	EShellContentClass *shell_content_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_shell_content_set_property;
	object_class->get_property = mail_shell_content_get_property;
	object_class->dispose = mail_shell_content_dispose;
	object_class->finalize = mail_shell_content_finalize;
	object_class->constructed = mail_shell_content_constructed;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->new_search_context = em_search_context_new;
	shell_content_class->check_state = mail_shell_content_check_state;

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		g_param_spec_boolean (
			"preview-visible",
			_("Preview is Visible"),
			_("Whether the preview pane is visible"),
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_VERTICAL_VIEW,
		g_param_spec_boolean (
			"vertical-view",
			_("Vertical View"),
			_("Whether vertical view is enabled"),
			FALSE,
			G_PARAM_READWRITE));
}

static void
mail_shell_content_init (EMailShellContent *mail_shell_content)
{
	mail_shell_content->priv =
		E_MAIL_SHELL_CONTENT_GET_PRIVATE (mail_shell_content);

	mail_shell_content->priv->preview_visible = TRUE;

	/* Postpone widget construction until we have a shell view. */
}

GType
e_mail_shell_content_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailShellContentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_shell_content_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailShellContent),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_shell_content_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_CONTENT, "EMailShellContent",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_mail_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MAIL_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

gboolean
e_mail_shell_content_get_preview_visible (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), FALSE);

	return mail_shell_content->priv->preview_visible;
}

void
e_mail_shell_content_set_preview_visible (EMailShellContent *mail_shell_content,
                                          gboolean preview_visible)
{
	GtkPaned *paned;
	GtkWidget *child;

	g_return_if_fail (E_IS_MAIL_SHELL_CONTENT (mail_shell_content));

	if (preview_visible == mail_shell_content->priv->preview_visible)
		return;

	paned = GTK_PANED (mail_shell_content->priv->paned);
	child = gtk_paned_get_child2 (paned);

	if (preview_visible)
		gtk_widget_show (child);
	else
		gtk_widget_hide (child);

	mail_shell_content->priv->preview_visible = preview_visible;

	g_object_notify (G_OBJECT (mail_shell_content), "preview-visible");
}

gboolean
e_mail_shell_content_get_vertical_view (EMailShellContent *mail_shell_content)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_CONTENT (mail_shell_content), FALSE);

	return mail_shell_content->priv->vertical_view;
}

void
e_mail_shell_content_set_vertical_view (EMailShellContent *mail_shell_content,
                                        gboolean vertical_view)
{
	GConfBridge *bridge;
	GtkWidget *old_paned;
	GtkWidget *new_paned;
	GtkWidget *child1;
	GtkWidget *child2;
	guint binding_id;
	const gchar *key;

	g_return_if_fail (E_IS_MAIL_SHELL_CONTENT (mail_shell_content));

	if (vertical_view == mail_shell_content->priv->vertical_view)
		return;

	bridge = gconf_bridge_get ();
	old_paned = mail_shell_content->priv->paned;
	binding_id = mail_shell_content->priv->paned_binding_id;

	child1 = gtk_paned_get_child1 (GTK_PANED (old_paned));
	child2 = gtk_paned_get_child2 (GTK_PANED (old_paned));

	if (binding_id > 0)
		gconf_bridge_unbind (bridge, binding_id);

	if (vertical_view) {
		new_paned = gtk_hpaned_new ();
		key = "/apps/evolution/mail/display/hpaned_size";
	} else {
		new_paned = gtk_vpaned_new ();
		key = "/apps/evolution/mail/display/paned_size";
	}

	gtk_widget_reparent (child1, new_paned);
	gtk_widget_reparent (child2, new_paned);
	gtk_widget_show (new_paned);

	gtk_widget_destroy (old_paned);
	gtk_container_add (GTK_CONTAINER (mail_shell_content), new_paned);

	binding_id = gconf_bridge_bind_property_delayed (
		bridge, key, G_OBJECT (new_paned), "position");

	mail_shell_content->priv->vertical_view = vertical_view;
	mail_shell_content->priv->paned_binding_id = binding_id;
	mail_shell_content->priv->paned = g_object_ref (new_paned);

	g_object_notify (G_OBJECT (mail_shell_content), "vertical-view");
}
