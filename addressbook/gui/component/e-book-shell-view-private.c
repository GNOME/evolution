/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-book-shell-view-private.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-book-shell-view-private.h"

void
e_book_shell_view_private_init (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	GHashTable *uid_to_view;
	GHashTable *uid_to_editor;
	GtkWidget *widget;

	uid_to_view = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	uid_to_editor = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	priv->contact_actions = gtk_action_group_new ("contacts");

	e_book_shell_view_actions_init (book_shell_view);

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	priv->notebook = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	e_book_get_addressbooks (&priv->source_list, NULL);
}

void
e_book_shell_view_private_dispose (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	DISPOSE (priv->notebook);
	DISPOSE (priv->source_list);

	g_hash_table_remove_all (priv->uid_to_view);
	g_hash_table_remove_all (priv->uid_to_editor);
}

void
e_book_shell_view_private_finalize (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	g_hash_table_destroy (priv->uid_to_view);
	g_hash_table_destroy (priv->uid_to_editor);
}

EABView *
e_book_shell_view_get_current_view (EBookShellView *book_shell_view)
{
	GtkNotebook *notebook;
	GtkWidget *widget;
	gint page_num;

	g_return_val_if_fail (E_IS_BOOK_SHELL_VIEW (book_shell_view), NULL);

	notebook = GTK_NOTEBOOK (book_shell_view->priv->notebook);
	page_num = gtk_notebook_get_current_page (notebook);
	widget = gtk_notebook_get_nth_page (notebook, page_num);

	return EAB_VIEW (widget);
}
