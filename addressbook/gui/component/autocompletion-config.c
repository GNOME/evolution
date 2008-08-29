/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-config-autocompletion.h - Configuration page for addressbook autocompletion.
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

#include "autocompletion-config.h"

#include <e-shell.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <e-preferences-window.h>
#include <libedataserver/e-source-list.h>
#include <libedataserverui/e-source-selector.h>

static void
source_selection_changed_cb (ESourceSelector *source_selector)
{
	ESourceList *source_list;
	GSList *selection;
	GSList *l;
	GSList *groups;

	source_list = e_source_selector_get_source_list (source_selector);

	/* first we clear all the completion flags from all sources */
	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;

		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);

			e_source_set_property (source, "completion", NULL);
		}
	}

	/* then we loop over the selector's selection, setting the
	   property on those sources */
	selection = e_source_selector_get_selection (source_selector);
	for (l = selection; l; l = l->next) {
		ESource *source = E_SOURCE (l->data);

		e_source_set_property (source, "completion", "true");
	}
	e_source_selector_free_selection (selection);

	/* XXX we should pop up a dialog if this fails */
	e_source_list_sync (source_list, NULL);
}

static void
initialize_selection (ESourceSelector *source_selector)
{
	ESourceList *source_list;
	GSList *groups;

	source_list = e_source_selector_get_source_list (source_selector);

	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;

		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);
			const char *completion;

			completion = e_source_get_property (source, "completion");
			if (completion && !g_ascii_strcasecmp (completion, "true"))
				e_source_selector_select_source (source_selector, source);
		}
	}
}

void
autocompletion_config_init (void)
{
	ESourceList *source_list;
	GtkWidget *scrolled_window;
	GtkWidget *source_selector;
	GtkWidget *preferences_window;

	source_list = e_source_list_new_for_gconf_default (
		"/apps/evolution/addressbook/sources");

	/* XXX should we watch for the source list to change and
	   update it in the control?  what about our local changes? */
	/*	g_signal_connect (ac->source_list, "changed", G_CALLBACK (source_list_changed), ac); */

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_widget_show (scrolled_window);

	source_selector = e_source_selector_new (source_list);
	g_signal_connect (
		source_selector, "selection_changed",
		G_CALLBACK (source_selection_changed_cb), NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), source_selector);
	gtk_widget_show (source_selector);

	initialize_selection (E_SOURCE_SELECTOR (source_selector));

	e_preferences_window_add_page (
		e_shell_get_preferences_window (),
		"autocompletion",
		"preferences-autocompletion",
		_("Autocompletion"),
		scrolled_window,
		200);
}
