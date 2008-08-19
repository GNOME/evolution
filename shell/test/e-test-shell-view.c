/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-test-shell-view.c
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

#include "e-test-shell-view.h"

#include <glib/gi18n.h>

#define E_TEST_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TEST_SHELL_VIEW, ETestShellViewPrivate))

struct _ETestShellViewPrivate {
	gint dummy_value;
};

GType e_test_shell_view_type = 0;
static gpointer parent_class;

static void
test_shell_view_class_init (ETestShellViewClass *class,
                            GTypeModule *module)
{
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ETestShellViewPrivate));

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = N_("Test");
	shell_view_class->icon_name = "face-monkey";
	shell_view_class->module = module;
}

static void
test_shell_view_init (ETestShellView *test_view)
{
	test_view->priv = E_TEST_SHELL_VIEW_GET_PRIVATE (test_view);
}

GType
e_test_shell_view_get_type (GTypeModule *module)
{
	if (e_test_shell_view_type == 0) {
		const GTypeInfo type_info = {
			sizeof (ETestShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) test_shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			module,  /* class_data */
			sizeof (ETestShellView),
			0,       /* n_preallocs */
			(GInstanceInitFunc) test_shell_view_init,
			NULL     /* value_table */
		};

		e_test_shell_view_type =
			g_type_module_register_type (
				module, E_TYPE_SHELL_VIEW,
				"ETestShellView", &type_info, 0);
	}

	return e_test_shell_view_type;
}
