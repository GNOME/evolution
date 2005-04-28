/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Author: Michael Zucchi <notzed@novell.com>
 *
 *  Copyright 2005 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <e-util/e-profile-event.h>

void
org_gnome_evolution_profiler_event(EPlugin *ep, EProfileEventTarget *t)
{
	static FILE *fp;

	if (!fp) {
		char *name;

		name = g_strdup_printf("eprofile.%ld", (long int)getpid());
		fp = fopen(name, "w");
		if (fp)
			fprintf(stderr, "Generating profiling data in `%s'\n", name);
		g_free(name);
	}

	if (fp)
		fprintf(fp, "%d.%d: %s,%s\n", t->tv.tv_sec, t->tv.tv_usec, t->id, t->uid);
}
