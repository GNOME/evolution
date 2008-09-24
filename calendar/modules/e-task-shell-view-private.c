/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-shell-view-private.c
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

#include "e-task-shell-view-private.h"

#include <widgets/menus/gal-view-factory-etable.h>

static void
task_shell_view_load_view_collection (EShellViewClass *shell_view_class)
{
	GalViewCollection *collection;
	GalViewFactory *factory;
	ETableSpecification *spec;
	const gchar *base_dir;
	gchar *filename;

	collection = shell_view_class->view_collection;

	base_dir = EVOLUTION_ETSPECDIR;
	spec = e_table_specification_new ();
	filename = g_build_filename (base_dir, ETSPEC_FILENAME, NULL);
	if (!e_table_specification_load_from_file (spec, filename))
		g_critical ("Unable to load ETable specification file "
			    "for tasks");
	g_free (filename);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	gal_view_collection_load (collection);
}

static void
task_shell_view_notify_view_id_cb (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	GalViewInstance *view_instance;
	const gchar *view_id;

	task_shell_content = task_shell_view->priv->task_shell_content;
	view_instance =
		e_task_shell_content_get_view_instance (task_shell_content);
	view_id = e_shell_view_get_view_id (E_SHELL_VIEW (task_shell_view));

	/* A NULL view ID implies we're in a custom view.  But you can
	 * only get to a custom view via the "Define Views" dialog, which
	 * would have already modified the view instance appropriately.
	 * Furthermore, there's no way to refer to a custom view by ID
	 * anyway, since custom views have no IDs. */
	if (view_id == NULL)
		return;

	gal_view_instance_set_current_view_id (view_instance, view_id);
}

void
e_task_shell_view_private_init (ETaskShellView *task_shell_view,
                                EShellViewClass *shell_view_class)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
	ESourceList *source_list;
	GObject *object;

	object = G_OBJECT (shell_view_class->type_module);
	source_list = g_object_get_data (object, "source-list");
	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	priv->source_list = g_object_ref (source_list);
	priv->task_actions = gtk_action_group_new ("tasks");

	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		task_shell_view_load_view_collection (shell_view_class);

	g_signal_connect (
		task_shell_view, "notify::view-id",
		G_CALLBACK (task_shell_view_notify_view_id_cb), NULL);
}

void
e_task_shell_view_private_constructed (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	/* Cache these to avoid lots of awkward casting. */
	priv->task_shell_content = g_object_ref (shell_content);
	priv->task_shell_sidebar = g_object_ref (shell_sidebar);
}

void
e_task_shell_view_private_dispose (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;

	DISPOSE (priv->source_list);

	DISPOSE (priv->task_actions);

	DISPOSE (priv->task_shell_content);
	DISPOSE (priv->task_shell_sidebar);
}

void
e_task_shell_view_private_finalize (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
}

void
e_task_shell_view_set_status_message (ETaskShellView *task_shell_view,
                                      const gchar *status_message)
{
	g_return_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view));

	/* FIXME */
}
