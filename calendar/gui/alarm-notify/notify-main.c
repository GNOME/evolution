/*
 * Evolution calendar - Alarm notification service main file
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <unique/unique.h>
#include <camel/camel.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-passwords.h>

#include "e-util/e-util-private.h"
#include "alarm.h"
#include "alarm-queue.h"
#include "alarm-notify.h"
#include "config-data.h"

gint
main (gint argc, gchar **argv)
{
	AlarmNotify *alarm_notify_service;
	UniqueApp *app;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_thread_init (NULL);
	dbus_g_thread_init ();

	gtk_init (&argc, &argv);

	app = unique_app_new ("org.gnome.EvolutionAlarmNotify", NULL);

	if (unique_app_is_running (app))
		goto exit;

	alarm_notify_service = alarm_notify_new ();

	/* FIXME Ideally we should not use camel libraries in calendar,
	 *       though it is the case currently for attachments. Remove
	 *       this once that is fixed. */

	/* Initialize Camel's type system. */
	camel_object_get_type();

	gtk_main ();

	if (alarm_notify_service != NULL)
		g_object_unref (alarm_notify_service);

	alarm_done ();

	e_passwords_shutdown ();

exit:
	g_object_unref (app);

	return 0;
}
