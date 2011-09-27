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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libebook/e-book-client.h>
#include <libecal/e-cal-client.h>
#include <libedataserver/e-source.h>

#include <addressbook/gui/widgets/eab-config.h>
#include <calendar/gui/e-cal-config.h>
#include <e-util/e-config.h>
#include <e-util/e-plugin-util.h>
#include <e-util/e-plugin-util.h>
#include <shell/e-shell.h>
#include <shell/e-shell-backend.h>

GtkWidget *org_gnome_default_book (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *org_gnome_autocomplete_book (EPlugin *epl, EConfigHookItemFactoryData *data);

void commit_default_calendar (EPlugin *epl, EConfigTarget *target);
void commit_default_book (EPlugin *epl, EConfigTarget *target);

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

static void
mark_default_source_in_list (ESourceList *source_list,
                             ESource *source)
{
	GSList *g, *s;
	g_return_if_fail (source_list != NULL);
	g_return_if_fail (source != NULL);

	source = e_source_list_peek_source_by_uid (source_list, e_source_peek_uid (source));

	for (g = e_source_list_peek_groups (source_list); g; g = g->next) {
		ESourceGroup *group = g->data;

		for (s = e_source_group_peek_sources (group); s; s = s->next) {
			ESource *es = s->data;

			e_source_set_property (es, "default", es == source ? "true" : NULL);
		}
	}
}

void
commit_default_calendar (EPlugin *epl,
                         EConfigTarget *target)
{
	ECalConfigTargetSource *cal_target;
	ESource *source;

	cal_target = (ECalConfigTargetSource *) target;
	source = cal_target->source;
	if (e_source_get_property (source, "default")) {
		EShellBackend *shell_backend = NULL;
		ESourceList *source_list = NULL;

		switch (cal_target->source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			shell_backend = e_shell_get_backend_by_name (e_shell_get_default (), "calendar");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			shell_backend = e_shell_get_backend_by_name (e_shell_get_default (), "memos");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			shell_backend = e_shell_get_backend_by_name (e_shell_get_default (), "tasks");
			break;
		default:
			break;
		}

		if (shell_backend)
			g_object_get (G_OBJECT (shell_backend), "source-list", &source_list, NULL);

		if (source_list) {
			/* mark in the backend's source_list, to avoid race
			 * with saving of two different source lists
			*/
			mark_default_source_in_list (source_list, source);
		} else {
			GError *error = NULL;

			e_cal_client_set_default_source (source, cal_target->source_type, &error);
			if (error)
				g_debug ("%s: Failed to set default source: %s", G_STRFUNC, error->message);
			g_clear_error (&error);
		}
	}
}

void
commit_default_book (EPlugin *epl,
                     EConfigTarget *target)
{
	EABConfigTargetSource *book_target;
	ESource *source;

	book_target = (EABConfigTargetSource *) target;
	source = book_target->source;
	if (e_source_get_property (source, "default")) {
		EShellBackend *shell_backend;
		ESourceList *source_list = NULL;

		shell_backend = e_shell_get_backend_by_name (e_shell_get_default (), "addressbook");
		if (shell_backend)
			g_object_get (G_OBJECT (shell_backend), "source-list", &source_list, NULL);

		if (source_list) {
			/* mark in the backend's source_list, to avoid race
			 * with saving of two different source lists
			*/
			mark_default_source_in_list (source_list, source);
		} else {
			GError *error = NULL;

			e_book_client_set_default_source (source, &error);
			if (error)
				g_debug ("%s: Failed to set default source: %s", G_STRFUNC, error->message);
			g_clear_error (&error);
		}
	}

}

GtkWidget *
org_gnome_default_book (EPlugin *epl,
                        EConfigHookItemFactoryData *data)
{
	EABConfigTargetSource *book_target;

	if (data->old)
		return data->old;

	book_target = (EABConfigTargetSource *) data->target;

	return e_plugin_util_add_check (data->parent, _("Mark as _default address book"), book_target->source, "default", "true", NULL);
}

GtkWidget *
org_gnome_autocomplete_book (EPlugin *epl,
                             EConfigHookItemFactoryData *data)
{
	EABConfigTargetSource *book_target;

	if (data->old)
		return data->old;

	book_target = (EABConfigTargetSource *) data->target;

	return e_plugin_util_add_check (data->parent, _("A_utocomplete with this address book"), book_target->source, "completion", "true", NULL);
}

static const gchar *
get_calendar_option_caption (ECalClientSourceType source_type)
{
	const gchar *res = "???";

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS: res = _("Mark as _default calendar"); break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:  res = _("Mark as _default task list"); break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:  res = _("Mark as _default memo list"); break;
		default: break;
	}

	return res;
}

GtkWidget *org_gnome_default_cal (EPlugin *epl, EConfigHookItemFactoryData *data);

GtkWidget *
org_gnome_default_cal (EPlugin *epl,
                       EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *cal_target;

	if (data->old)
		return data->old;

	cal_target = (ECalConfigTargetSource *) data->target;

	return e_plugin_util_add_check (data->parent, get_calendar_option_caption (cal_target->source_type), cal_target->source, "default", "true", NULL);
}
