/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-config-autocompletion.h - Configuration page for addressbook autocompletion.
 *
 * Copyright (C) 2003 Novell, Inc.
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
 * Authors: Chris Toshok <toshok@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "autocompletion-config.h"

#include "Evolution.h"

#include <bonobo/bonobo-exception.h>

#include "e-source-selector.h"
#include <libedataserver/e-source-list.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtksignal.h>


typedef struct {
	EvolutionConfigControl *config_control;

	GtkWidget *control_widget;

	ESourceList *source_list;
} AutocompletionConfig;

static void
source_selection_changed (ESourceSelector *selector,
			  AutocompletionConfig *ac)
{
	GSList *selection;
	GSList *l;
	GSList *groups;

	/* first we clear all the completion flags from all sources */
	for (groups = e_source_list_peek_groups (ac->source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);

			e_source_set_property (source, "completion", NULL);
		}
	}

	/* then we loop over the selector's selection, setting the
	   property on those sources */
	selection = e_source_selector_get_selection (selector);
	for (l = selection; l; l = l->next) {
		e_source_set_property (E_SOURCE (l->data), "completion", "true");
	}
	e_source_selector_free_selection (selection);

	e_source_list_sync (ac->source_list, NULL); /* XXX we should pop up a dialog if this fails */
}

static void
config_control_destroy_notify (void *data,
			       GObject *where_the_config_control_was)
{
	AutocompletionConfig *ac = (AutocompletionConfig *) data;

	g_object_unref (ac->source_list);

	g_free (ac);
}

static void
initialize_selection (AutocompletionConfig *ac)
{
	GSList *groups;

	for (groups = e_source_list_peek_groups (ac->source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);
			const char *completion = e_source_get_property (source, "completion");
			if (completion && !g_ascii_strcasecmp (completion, "true"))
				e_source_selector_select_source (E_SOURCE_SELECTOR (ac->control_widget),
								 source);
		}
	}
}

EvolutionConfigControl*
autocompletion_config_control_new (void)
{
	AutocompletionConfig *ac;
	CORBA_Environment ev;
	GtkWidget *scrolledwin;

	ac = g_new0 (AutocompletionConfig, 1);

	CORBA_exception_init (&ev);

	ac->source_list =  e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");
	/* XXX should we watch for the source list to change and
	   update it in the control?  what about our local changes? */
	/*	g_signal_connect (ac->source_list, "changed", G_CALLBACK (source_list_changed), ac); */

	scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwin),
					     GTK_SHADOW_IN);

	ac->control_widget = e_source_selector_new (ac->source_list);
	e_source_selector_set_toggle_selection((ESourceSelector *)ac->control_widget, TRUE);

	gtk_container_add (GTK_CONTAINER (scrolledwin), ac->control_widget);

	initialize_selection (ac);

	gtk_widget_show (ac->control_widget);
	gtk_widget_show (scrolledwin);

	ac->config_control = evolution_config_control_new (scrolledwin);

	g_signal_connect (ac->control_widget, "selection_changed",
			  G_CALLBACK (source_selection_changed), ac);
	
	g_object_weak_ref (G_OBJECT (ac->config_control), config_control_destroy_notify, ac);

	CORBA_exception_free (&ev);

	return ac->config_control;
}

