/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-uid.c - Unique ID generator.
 *
 * Copyright (C) 2002 Ximian, Inc.
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
 * Author: Dan Winship <danw@ximian.com>
 */

#include "e-uid.h"

#include <glib/gstrfuncs.h>

#include <string.h>
#include <time.h>
#include <unistd.h>


/**
 * e_uid_new:
 * 
 * Generate a new unique string for use e.g. in account lists.
 * 
 * Return value: the newly generated UID.  The caller should free the string
 * when it's done with it.
 **/
char *
e_uid_new (void)
{
	static char *hostname;
	static int serial;

	if (!hostname) {
		static char buffer [512];

		if ((gethostname (buffer, sizeof (buffer) - 1) == 0) &&
		    (buffer [0] != 0))
			hostname = buffer;
		else
			hostname = "localhost";
	}

	return g_strdup_printf ("%lu.%lu.%d@%s",
				(unsigned long) time (NULL),
				(unsigned long) getpid (),
				serial++,
				hostname);
}
