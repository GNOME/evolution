/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-host-utils.c
 *
 * Copyright (C) 2001  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Chris Toshok
 */

#include <config.h>
#include <glib.h>
#include "e-msgport.h"
#include "e-host-utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifndef HAVE_GETHOSTBYNAME_R
static EMutex *gethost_mutex = NULL;
#endif

void
e_gethostbyname_init ()
{
#ifndef HAVE_GETHOSTBYNAME_R
	if (gethost_mutex)
		return;

	gethost_mutex = e_mutex_new (E_MUTEX_SIMPLE);
#endif
}

int
e_gethostbyname_r (const char *name, struct hostent *host,
		   char *buf, int buflen, int *herr)
{
#ifdef HAVE_GETHOSTBYNAME_R
#ifdef GETHOSTBYNAME_R_FIVE_ARGS
	return gethostbyname_r(name, host, buf, buflen, herr);
#else
	struct hostent *hp;
	return gethostbyname_r(name, host, buf, buflen, &hp, herr);
#endif
#else
	int i;
	char *p;
	struct hostent *h;
	int req_length;
	int num_aliases = 0, num_addrs = 0;

	if (!gethost_mutex) {
		g_warning ("mutex wasn't initialized - you must call e_gethostbyname_init before e_gethostbyname_r\n");
		return -1;
	}

	e_mutex_lock (gethost_mutex);

	h = gethostbyname (name);

	if (!h) {
		e_mutex_unlock (gethost_mutex);
		return -1;
	}

	/* check to make sure we have enough room in our buffer */
	req_length = 0;
	if (h->h_aliases) {
		for (i = 0; h->h_aliases[i]; i ++)
			req_length += strlen (h->h_aliases[i]) + 1;
		num_aliases = i + 1;
	}
	if (h->h_addr_list) {
		for (i = 0; h->h_addr_list[i]; i ++)
			req_length += h->h_length;
		num_addrs = i + 1;
	}

	if (buflen < req_length) {
		*herr = ERANGE;
		e_mutex_unlock (gethost_mutex);
		return -1;
	}

	if (num_aliases)
		host->h_aliases = malloc (sizeof (char*) * num_aliases);
	else
		host->h_aliases = NULL;
	if (num_addrs)
		host->h_addr_list = malloc (sizeof (char*) * num_addrs);
	else
		host->h_addr_list = NULL;

	host->h_name = strdup (h->h_name);
	host->h_addrtype = h->h_addrtype;
	host->h_length = h->h_length;

	/* copy the aliases/addresses into the buffer, and assign the
	   pointers into the hostent */
	*buf = 0;
	p = buf;
	if (num_aliases) {
		for (i = 0; h->h_aliases[i]; i ++) {
			strcpy (buf, h->h_aliases[i]);
			host->h_aliases[i] = p;
			p += strlen (h->h_aliases[i]);
		}
		host->h_aliases[num_aliases - 1] = NULL;
	}

	if (num_addrs) {
		for (i = 0; h->h_addr_list[i]; i ++) {
			memcpy (buf, h->h_addr_list[i], h->h_length);
			host->h_addr_list[i] = p;
			p += h->h_length;
		}
		host->h_addr_list[num_addrs - 1] = NULL;
	}

	*herr = h_errno;

	e_mutex_unlock (gethost_mutex);

	return 0;
#endif
}
