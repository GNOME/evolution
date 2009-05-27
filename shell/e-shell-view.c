/*
 * Helper class for evolution shells to setup a view
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
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-shell-view.h"
#include "e-shell-window.h"

static BonoboObjectClass *parent_class = NULL;

struct _EShellViewPrivate {
	gint dummy;
};

static void
impl_ShellView_setTitle(PortableServer_Servant _servant, const CORBA_char *id, const CORBA_char * title, CORBA_Environment * ev)
{
	EShellView *esw = (EShellView *)bonobo_object_from_servant(_servant);
	/* To translators: This is the window title and %s is the
	component name. Most translators will want to keep it as is. */
	gchar *tmp = g_strdup_printf(_("%s - Evolution"), title);

	e_shell_window_set_title(esw->window, id, tmp);
	g_free(tmp);
}

static void
impl_ShellView_setComponent(PortableServer_Servant _servant, const CORBA_char *id, CORBA_Environment * ev)
{
	EShellView *esw = (EShellView *)bonobo_object_from_servant(_servant);

	e_shell_window_switch_to_component(esw->window, id);
}

struct change_icon_struct {
	const gchar *component_name;
	const gchar *icon_name;
};

static gboolean
change_button_icon_func (EShell *shell, EShellWindow *window, gpointer user_data)
{
	struct change_icon_struct *cis = (struct change_icon_struct*)user_data;

	g_return_val_if_fail (window != NULL, FALSE);
	g_return_val_if_fail (cis != NULL, FALSE);

	e_shell_window_change_component_button_icon (window, cis->component_name, cis->icon_name);

	return TRUE;
}

static void
impl_ShellView_setButtonIcon (PortableServer_Servant _servant, const CORBA_char *id, const CORBA_char * iconName, CORBA_Environment * ev)
{
	EShellView *esw = (EShellView *)bonobo_object_from_servant(_servant);
	EShell *shell = e_shell_window_peek_shell (esw->window);

	struct change_icon_struct cis;
	cis.component_name = id;
	cis.icon_name = iconName;

	e_shell_foreach_shell_window (shell, change_button_icon_func, &cis);
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
	epv->setButtonIcon = impl_ShellView_setButtonIcon;
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

