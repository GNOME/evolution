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
#include <config.h>
#endif

#include "anjal-mail-view.h"
#include <glib/gi18n.h>
#include "mail/em-utils.h"
#include "mail/mail-send-recv.h"
#include "libemail-engine/mail-ops.h"
#include "mail/em-folder-tree.h"

G_DEFINE_TYPE (AnjalMailView, anjal_mail_view, GTK_TYPE_NOTEBOOK)

static void
view_set_folder_uri (AnjalMailView *mail_view,
                     const gchar *uri)
{
}

static void
view_set_folder_tree_widget (AnjalMailView *mail_view,
                             GtkWidget *tree)
{
}

static void
view_set_folder_tree (AnjalMailView *mail_view,
                      EMFolderTree *tree)
{
}

static void
view_set_search (AnjalMailView *mail_view,
                 const gchar *search)
{
}

static void
view_init_search (AnjalMailView *mail_view,
                  GtkWidget *search)
{
}

static void
anjal_mail_view_class_init (AnjalMailViewClass *class)
{
	class->set_folder_uri = view_set_folder_uri;
	class->set_folder_tree_widget = view_set_folder_tree_widget;
	class->set_folder_tree = view_set_folder_tree;
	class->set_search = view_set_search;
	class->init_search = view_init_search;
};

static void
anjal_mail_view_init (AnjalMailView *shell)
{
}

