/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-test-shell-view.h
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

#ifndef E_TEST_SHELL_VIEW_H
#define E_TEST_SHELL_VIEW_H

#include <e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_TEST_SHELL_VIEW \
	(e_test_shell_view_type)
#define E_TEST_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TEST_SHELL_VIEW, ETestShellView))
#define E_TEST_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TEST_SHELL_VIEW, ETestShellViewClass))
#define E_IS_TEST_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TEST_SHELL_VIEW))
#define E_IS_TEST_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TEST_SHELL_VIEW))
#define E_TEST_SHELL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TEST_SHELL_VIEW, ETestShellViewClass))

G_BEGIN_DECLS

extern GType e_test_shell_view_type;

typedef struct _ETestShellView ETestShellView;
typedef struct _ETestShellViewClass ETestShellViewClass;
typedef struct _ETestShellViewPrivate ETestShellViewPrivate;

struct _ETestShellView {
	EShellView parent;
	ETestShellViewPrivate *priv;
};

struct _ETestShellViewClass {
	EShellViewClass parent_class;
};

GType		e_test_shell_view_get_type	(GTypeModule *module);

G_END_DECLS

#endif /* E_TEST_SHELL_VIEW_H */
