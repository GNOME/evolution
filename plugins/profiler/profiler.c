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
 *		Michael Zucchi <notzed@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <e-util/e-profile-event.h>

void
org_gnome_evolution_profiler_event(EPlugin *ep, EProfileEventTarget *t)
{
	static FILE *fp;

	if (!fp) {
		gchar *name;

		name = g_strdup_printf("eprofile.%ld", (glong)getpid());
		fp = fopen(name, "w");
		if (fp)
			fprintf(stderr, "Generating profiling data in `%s'\n", name);
		g_free(name);
	}

	if (fp)
		fprintf(fp, "%d.%d: %s,%s\n", t->tv.tv_sec, t->tv.tv_usec, t->id, t->uid);
}
