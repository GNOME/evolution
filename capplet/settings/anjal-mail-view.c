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
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 2009 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "anjal-mail-view.h"
#include <glib/gi18n.h>
#include "mail/em-utils.h"
#include "mail/mail-send-recv.h"
#include "mail/mail-ops.h"
#include "mail/em-folder-tree.h"

struct  _AnjalMailViewPrivate {

	gboolean started;
};

G_DEFINE_TYPE (AnjalMailView, anjal_mail_view, GTK_TYPE_NOTEBOOK)

static void
anjal_mail_view_init (AnjalMailView  *shell)
{
	shell->priv = g_new0(AnjalMailViewPrivate, 1);
	shell->priv->started = TRUE;
}

static void
anjal_mail_view_finalize (GObject *object)
{
	AnjalMailView *shell = (AnjalMailView *)object;
	AnjalMailViewPrivate *priv = shell->priv;

	g_free (priv);

	G_OBJECT_CLASS (anjal_mail_view_parent_class)->finalize (object);
}

static void
view_set_folder_uri (AnjalMailView *mail_view, const gchar *uri)
{
}
static void
view_set_folder_tree_widget (AnjalMailView *mail_view, GtkWidget *tree)
{
}
static void
view_set_folder_tree (AnjalMailView *mail_view, EMFolderTree *tree)
{
}

static void
view_set_search (AnjalMailView *mail_view, const gchar *search)
{
}

static void
view_init_search (AnjalMailView *mail_view, GtkWidget *search)
{
}

static void
anjal_mail_view_class_init (AnjalMailViewClass *klass)
{
	GObjectClass * object_class = G_OBJECT_CLASS (klass);

	anjal_mail_view_parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = anjal_mail_view_finalize;
	klass->set_folder_uri = view_set_folder_uri;
	klass->set_folder_tree_widget = view_set_folder_tree_widget;
	klass->set_folder_tree = view_set_folder_tree;
	klass->set_search = view_set_search;
	klass->init_search = view_init_search;
};

AnjalMailView *
anjal_mail_view_new ()
{
	AnjalMailView *shell = g_object_new (ANJAL_MAIL_VIEW_TYPE, NULL);

	return shell;
}

void
anjal_mail_view_set_folder_uri (AnjalMailView *mv, const gchar *uri)
{
	if (!mv || !uri)
		return;

	ANJAL_MAIL_VIEW_GET_CLASS(mv)->set_folder_uri (mv, uri);
}

void
anjal_mail_view_set_folder_tree_widget (AnjalMailView *mv, GtkWidget *tree)
{
	ANJAL_MAIL_VIEW_GET_CLASS(mv)->set_folder_tree_widget (mv, tree);
}

void
anjal_mail_view_set_folder_tree (AnjalMailView *mv, GtkWidget *tree)
{
	ANJAL_MAIL_VIEW_GET_CLASS(mv)->set_folder_tree (mv, (EMFolderTree *)tree);
}

void
anjal_mail_view_set_search (AnjalMailView *view, const gchar *search)
{
	ANJAL_MAIL_VIEW_GET_CLASS(view)->set_search (view, search);
}

void
anjal_mail_view_init_search (AnjalMailView *mv, GtkWidget *search)
{
	ANJAL_MAIL_VIEW_GET_CLASS(mv)->init_search (mv, search);
}

