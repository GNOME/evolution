/*
 * e-book-shell-view.c
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

#include "e-book-shell-view-private.h"

enum {
	PROP_0,
	PROP_SOURCE_LIST
};

GType e_book_shell_view_type = 0;
static gpointer parent_class;

static void
book_shell_view_source_list_changed_cb (EBookShellView *book_shell_view,
                                        ESourceList *source_list)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	EBookShellContent *book_shell_content;
	GList *keys, *iter;

	book_shell_content = book_shell_view->priv->book_shell_content;

	keys = g_hash_table_get_keys (priv->uid_to_view);
	for (iter = keys; iter != NULL; iter = iter->next) {
		gchar *uid = iter->data;
		EAddressbookView *view;

		/* If the source still exists, move on. */
		if (e_source_list_peek_source_by_uid (source_list, uid))
			continue;

		/* Remove the view for the deleted source. */
		view = g_hash_table_lookup (priv->uid_to_view, uid);
		e_book_shell_content_remove_view (book_shell_content, view);
		g_hash_table_remove (priv->uid_to_view, uid);
	}
	g_list_free (keys);

	keys = g_hash_table_get_keys (priv->uid_to_editor);
	for (iter = keys; iter != NULL; iter = iter->next) {
		gchar *uid = iter->data;
		EditorUidClosure *closure;

		/* If the source still exists, move on. */
		if (e_source_list_peek_source_by_uid (source_list, uid))
			continue;

		/* Remove the editor for the deleted source. */
		closure = g_hash_table_lookup (priv->uid_to_editor, uid);
		g_object_weak_unref (
			G_OBJECT (closure->editor), (GWeakNotify)
			e_book_shell_view_editor_weak_notify, closure);
		gtk_widget_destroy (closure->editor);
		g_hash_table_remove (priv->uid_to_editor, uid);
	}
	g_list_free (keys);

	e_book_shell_view_actions_update (book_shell_view);
}

static void
book_shell_view_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_LIST:
			g_value_set_object (
				value, e_book_shell_view_get_source_list (
				E_BOOK_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_shell_view_dispose (GObject *object)
{
	EBookShellView *book_shell_view;

	book_shell_view = E_BOOK_SHELL_VIEW (object);
	e_book_shell_view_private_dispose (book_shell_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
book_shell_view_finalize (GObject *object)
{
	EBookShellView *book_shell_view;

	book_shell_view = E_BOOK_SHELL_VIEW (object);
	e_book_shell_view_private_finalize (book_shell_view);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
book_shell_view_constructed (GObject *object)
{
	EBookShellView *book_shell_view;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	book_shell_view = E_BOOK_SHELL_VIEW (object);
	e_book_shell_view_private_constructed (book_shell_view);
}

static void
book_shell_view_class_init (EBookShellViewClass *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EBookShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = book_shell_view_get_property;
	object_class->dispose = book_shell_view_dispose;
	object_class->finalize = book_shell_view_finalize;
	object_class->constructed = book_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = N_("Contacts");
	shell_view_class->icon_name = "x-office-address-book";
	shell_view_class->ui_definition = "evolution-contacts.ui";
	shell_view_class->search_options = "/contact-search-options";
	shell_view_class->search_rules = "addresstypes.xml";
	shell_view_class->type_module = type_module;
	shell_view_class->new_shell_content = e_book_shell_content_new;
	shell_view_class->new_shell_sidebar = e_book_shell_sidebar_new;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_LIST,
		g_param_spec_object (
			"source-list",
			_("Source List"),
			_("The registry of address books"),
			E_TYPE_SOURCE_LIST,
			G_PARAM_READABLE));
}

static void
book_shell_view_init (EBookShellView *book_shell_view,
                      EShellViewClass *shell_view_class)
{
	book_shell_view->priv =
		E_BOOK_SHELL_VIEW_GET_PRIVATE (book_shell_view);

	e_book_shell_view_private_init (book_shell_view, shell_view_class);

	g_signal_connect_swapped (
		book_shell_view->priv->source_list, "changed",
		G_CALLBACK (book_shell_view_source_list_changed_cb),
		book_shell_view);
}

GType
e_book_shell_view_get_type (GTypeModule *type_module)
{
	if (e_book_shell_view_type == 0) {
		const GTypeInfo type_info = {
			sizeof (EBookShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) book_shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			type_module,
			sizeof (EBookShellView),
			0,    /* n_preallocs */
			(GInstanceInitFunc) book_shell_view_init,
			NULL  /* value_table */
		};

		e_book_shell_view_type =
			g_type_module_register_type (
				type_module, E_TYPE_SHELL_VIEW,
				"EBookShellView", &type_info, 0);
	}

	return e_book_shell_view_type;
}

ESourceList *
e_book_shell_view_get_source_list (EBookShellView *book_shell_view)
{
	g_return_val_if_fail (E_IS_BOOK_SHELL_VIEW (book_shell_view), NULL);

	return book_shell_view->priv->source_list;
}
