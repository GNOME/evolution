/*
 * e-mail-shell-view.c
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

#include "e-mail-shell-view-private.h"

enum {
	PROP_0
};

GType e_mail_shell_view_type = 0;
static gpointer parent_class;

static void
mail_shell_view_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_view_dispose (GObject *object)
{
	e_mail_shell_view_private_dispose (E_MAIL_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_shell_view_finalize (GObject *object)
{
	e_mail_shell_view_private_finalize (E_MAIL_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
mail_shell_view_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	e_mail_shell_view_private_constructed (E_MAIL_SHELL_VIEW (object));
}

static void
mail_shell_view_toggled (EShellView *shell_view)
{
	EMailShellViewPrivate *priv;
	EShellWindow *shell_window;
	GtkUIManager *ui_manager;
	const gchar *basename;
	gboolean view_is_active;

	/* Chain up to parent's toggled() method. */
	E_SHELL_VIEW_CLASS (parent_class)->toggled (shell_view);

	priv = E_MAIL_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	view_is_active = e_shell_view_is_active (shell_view);
	basename = E_MAIL_READER_UI_DEFINITION;

	if (view_is_active && priv->merge_id == 0) {
		priv->merge_id = e_load_ui_definition (ui_manager, basename);
		e_mail_reader_create_charset_menu (
			E_MAIL_READER (priv->mail_shell_content),
			ui_manager, priv->merge_id);
	} else if (!view_is_active && priv->merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager, priv->merge_id);
		priv->merge_id = 0;
	}

	gtk_ui_manager_ensure_update (ui_manager);
}

static void
mail_shell_view_update_actions (EShellView *shell_view)
{
	EMailShellViewPrivate *priv;
	EMailShellSidebar *mail_shell_sidebar;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EMFolderTree *folder_tree;
	GtkAction *action;
	CamelURL *camel_url;
	gchar *uri;
	gboolean sensitive;
	gboolean visible;
	guint32 state;

	/* Be descriptive. */
	gboolean folder_allows_children;
	gboolean folder_can_be_deleted;
	gboolean folder_is_junk;
	gboolean folder_is_outbox;
	gboolean folder_is_store;
	gboolean folder_is_trash;

	priv = E_MAIL_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_sidebar = priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	state = e_shell_sidebar_check_state (shell_sidebar);

	folder_allows_children =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_ALLOWS_CHILDREN);
	folder_can_be_deleted =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_CAN_DELETE);
	folder_is_junk =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_IS_JUNK);
	folder_is_outbox =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_IS_OUTBOX);
	folder_is_store =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_IS_STORE);
	folder_is_trash =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_IS_TRASH);

	uri = em_folder_tree_get_selected_uri (folder_tree);
	camel_url = camel_url_new (uri, NULL);
	g_free (uri);

	action = ACTION (MAIL_EMPTY_TRASH);
	visible = folder_is_trash;
	gtk_action_set_visible (action, visible);

	action = ACTION (MAIL_FLUSH_OUTBOX);
	visible = folder_is_outbox;
	gtk_action_set_visible (action, visible);

	action = ACTION (MAIL_FOLDER_COPY);
	sensitive = !folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_DELETE);
	sensitive = !folder_is_store && folder_can_be_deleted;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_MOVE);
	sensitive = !folder_is_store && folder_can_be_deleted;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_NEW);
	sensitive = folder_allows_children;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_PROPERTIES);
	sensitive = !folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_REFRESH);
	sensitive = !folder_is_store;
	visible = !folder_is_outbox;
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_visible (action, visible);

	action = ACTION (MAIL_FOLDER_RENAME);
	sensitive = !folder_is_store && folder_can_be_deleted;
	gtk_action_set_sensitive (action, sensitive);
}

static void
mail_shell_view_class_init (EMailShellViewClass *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_shell_view_get_property;
	object_class->dispose = mail_shell_view_dispose;
	object_class->finalize = mail_shell_view_finalize;
	object_class->constructed = mail_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Mail");
	shell_view_class->icon_name = "evolution-mail";
	shell_view_class->ui_definition = "evolution-mail.ui";
	shell_view_class->search_options = "/mail-search-options";
	shell_view_class->search_rules = "searchtypes.xml";
	shell_view_class->type_module = type_module;
	shell_view_class->new_shell_content = e_mail_shell_content_new;
	shell_view_class->new_shell_sidebar = e_mail_shell_sidebar_new;
	shell_view_class->toggled = mail_shell_view_toggled;
	shell_view_class->update_actions = mail_shell_view_update_actions;
}

static void
mail_shell_view_init (EMailShellView *mail_shell_view,
                      EShellViewClass *shell_view_class)
{
	mail_shell_view->priv =
		E_MAIL_SHELL_VIEW_GET_PRIVATE (mail_shell_view);

	e_mail_shell_view_private_init (mail_shell_view, shell_view_class);
}

GType
e_mail_shell_view_get_type (GTypeModule *type_module)
{
	if (e_mail_shell_view_type == 0) {
		const GTypeInfo type_info = {
			sizeof (EMailShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			type_module,
			sizeof (EMailShellView),
			0,    /* n_preallocs */
			(GInstanceInitFunc) mail_shell_view_init,
			NULL  /* value_table */
		};

		e_mail_shell_view_type =
			g_type_module_register_type (
				type_module, E_TYPE_SHELL_VIEW,
				"EMailShellView", &type_info, 0);
	}

	return e_mail_shell_view_type;
}
