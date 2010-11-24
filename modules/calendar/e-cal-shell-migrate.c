/*
 * e-cal-shell-backend-migrate.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-cal-shell-migrate.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libebackend/e-dbhash.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-group.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-xml-hash-utils.h>

#include "e-util/e-util-private.h"
#include "calendar/gui/calendar-config.h"
#include "calendar/gui/calendar-config-keys.h"
#include "calendar/gui/e-cal-event.h"
#include "shell/e-shell.h"

#define LOCAL_BASE_URI "local:"
#define WEBCAL_BASE_URI "webcal://"
#define CONTACTS_BASE_URI "contacts://"
#define BAD_CONTACTS_BASE_URI "contact://"
#define PERSONAL_RELATIVE_URI "system"

static ESourceGroup *
create_calendar_contact_source (ESourceList *source_list)
{
	ESourceGroup *group;
	ESource *source;

	/* Create the contacts group */
	group = e_source_group_new (_("Contacts"), CONTACTS_BASE_URI);
	e_source_list_add_group (source_list, group, -1);

	source = e_source_new (_("Birthdays & Anniversaries"), "/");
	e_source_group_add_source (group, source, -1);
	g_object_unref (source);

	e_source_set_color_spec (source, "#FED4D3");
	e_source_group_set_readonly (group, TRUE);

	return group;
}

static void
create_calendar_sources (EShellBackend *shell_backend,
			 ESourceList *source_list,
			 ESourceGroup **on_this_computer,
			 ESource **personal_source,
			 ESourceGroup **on_the_web,
			 ESourceGroup **contacts)
{
	EShell *shell;
	EShellSettings *shell_settings;
	GSList *groups;
	ESourceGroup *group;

	*on_this_computer = NULL;
	*on_the_web = NULL;
	*contacts = NULL;
	*personal_source = NULL;

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;
		gchar *base_dir, *base_uri;

		base_dir = g_build_filename (e_shell_backend_get_data_dir (shell_backend), "local", NULL);
		base_uri = g_filename_to_uri (base_dir, NULL, NULL);

		for (g = groups; g; g = g->next) {

			group = E_SOURCE_GROUP (g->data);

			if (!strcmp (BAD_CONTACTS_BASE_URI, e_source_group_peek_base_uri (group)))
				e_source_group_set_base_uri (group, CONTACTS_BASE_URI);

			if (!strcmp (base_uri, e_source_group_peek_base_uri (group)))
				e_source_group_set_base_uri (group, LOCAL_BASE_URI);

			if (!*on_this_computer && !strcmp (LOCAL_BASE_URI,
				e_source_group_peek_base_uri (group)))
				*on_this_computer = g_object_ref (group);

			else if (!*on_the_web && !strcmp (WEBCAL_BASE_URI,
				e_source_group_peek_base_uri (group)))
				*on_the_web = g_object_ref (group);

			else if (!*contacts && !strcmp (CONTACTS_BASE_URI,
				e_source_group_peek_base_uri (group)))
				*contacts = g_object_ref (group);
		}

		g_free (base_dir);
		g_free (base_uri);
	}

	if (*on_this_computer) {
		/* make sure "Personal" shows up as a source under
		   this group */
		GSList *sources = e_source_group_peek_sources (*on_this_computer);
		GSList *s;
		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;
			if (!strcmp (PERSONAL_RELATIVE_URI, relative_uri)) {
				*personal_source = g_object_ref (source);
				break;
			}
		}
	} else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), LOCAL_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_this_computer = group;
	}

	if (!*personal_source) {
		gchar *primary_calendar;

		/* Create the default Person calendar */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (*on_this_computer, source, -1);

		primary_calendar = e_shell_settings_get_string (
			shell_settings, "cal-primary-calendar");

		if (!primary_calendar && !calendar_config_get_calendars_selected ()) {
			GSList selected;

			e_shell_settings_set_string (
				shell_settings, "cal-primary-calendar",
				e_source_peek_uid (source));

			selected.data = (gpointer)e_source_peek_uid (source);
			selected.next = NULL;
			calendar_config_set_calendars_selected (&selected);
		}

		g_free (primary_calendar);
		e_source_set_color_spec (source, "#BECEDD");
		*personal_source = source;
	}

	if (!*on_the_web) {
		/* Create the Webcal source group */
		group = e_source_group_new (_("On The Web"), WEBCAL_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_the_web = group;
	}

	if (!*contacts) {
		group = create_calendar_contact_source (source_list);

		*contacts = group;
	}
}

gboolean
e_cal_shell_backend_migrate (EShellBackend *shell_backend,
                            gint major,
                            gint minor,
                            gint micro,
                            GError **error)
{
	ESourceGroup *on_this_computer = NULL, *on_the_web = NULL, *contacts = NULL;
	ESource *personal_source = NULL;
	ESourceList *source_list;
	ECalEvent *ece;
	ECalEventTargetBackend *target;

	g_object_get (shell_backend, "source-list", &source_list, NULL);

	/* we call this unconditionally now - create_groups either
	   creates the groups/sources or it finds the necessary
	   groups/sources. */
	create_calendar_sources (
		shell_backend, source_list, &on_this_computer,
		&personal_source, &on_the_web, &contacts);

	e_source_list_sync (source_list, NULL);

	/** @Event: component.migration
	 * @Title: Migration step in component initialization
	 * @Target: ECalEventTargetComponent
	 *
	 * component.migration is emitted during the calendar component
	 * initialization process. This allows new calendar backend types
	 * to be distributed as an e-d-s backend and a plugin without
	 * reaching their grubby little fingers into migration.c
	 */
	/* Fire off migration event */
	ece = e_cal_event_peek ();
	target = e_cal_event_target_new_module (ece, shell_backend, source_list, 0);
	e_event_emit ((EEvent *) ece, "module.migration", (EEventTarget *) target);

	if (on_this_computer)
		g_object_unref (on_this_computer);
	if (on_the_web)
		g_object_unref (on_the_web);
	if (contacts)
		g_object_unref (contacts);
	if (personal_source)
		g_object_unref (personal_source);

	return TRUE;
}

