/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 * e-shell-view.c
 *
 * Copyright (C) 2004 Novell Inc.
 *
 * Author(s): Michael Zucchi <notzed@ximian.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Helper class for evolution shells to setup a view
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <gtk/gtkwindow.h>

#include "e-shell-view.h"
#include "e-shell-window.h"

static BonoboObjectClass *parent_class = NULL;

struct _EShellViewPrivate {
	int dummy;
};

static void
impl_ShellView_setTitle(PortableServer_Servant _servant, const CORBA_char *id, const CORBA_char * title, CORBA_Environment * ev)
{
	EShellView *esw = (EShellView *)bonobo_object_from_servant(_servant);
	char *tmp = g_strdup_printf("Evolution - %s", title);

	printf("shell view:setTitle '%s'\n", title);

	e_shell_window_set_title(esw->window, id, tmp);
	g_free(tmp);
}

static void
impl_ShellView_setComponent(PortableServer_Servant _servant, const CORBA_char *id, CORBA_Environment * ev)
{
	EShellView *esw = (EShellView *)bonobo_object_from_servant(_servant);

	e_shell_window_switch_to_component(esw->window, id);
}

static void
impl_dispose (GObject *object)
{
	/*EShellView *esv = (EShellView *)object;*/

	((GObjectClass *)parent_class)->dispose(object);
}

static void
impl_finalise (GObject *object)
{
	((GObjectClass *)parent_class)->finalize(object);
}

static void
e_shell_view_class_init (EShellViewClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_ShellView__epv *epv;

	parent_class = g_type_class_ref(bonobo_object_get_type());

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalise;

	epv = & klass->epv;
	epv->setTitle = impl_ShellView_setTitle;
	epv->setComponent = impl_ShellView_setComponent;
}

static void
e_shell_view_init (EShellView *shell)
{
}

EShellView *e_shell_view_new(struct _EShellWindow *window)
{
	EShellView *new = g_object_new (e_shell_view_get_type (), NULL);

	/* TODO: listen to destroy? */
	new->window = window;

	return new;
}

BONOBO_TYPE_FUNC_FULL (EShellView, GNOME_Evolution_ShellView, bonobo_object_get_type(), e_shell_view)

