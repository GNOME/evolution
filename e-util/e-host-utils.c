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
#include "e-host-utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

G_LOCK_DEFINE_STATIC (gethost_mutex);

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

	G_LOCK (gethost_mutex);

	h = gethostbyname (name);

	if (!h) {
		G_UNLOCK (gethost_mutex);
		return -1;
	}

	/* check to make sure we have enough room in our buffer */
	req_length = 0;
	if (h->h_aliases) {
		for (i = 0; h->h_aliases[i]; i ++)
			req_length += strlen (h->h_aliases[i]) + 1;
		num_aliases = i;
	}
	if (h->h_addr_list) {
		for (i = 0; h->h_addr_list[i]; i ++)
			req_length += h->h_length;
		num_addrs = i;
	}

	req_length += sizeof (char*) * (num_aliases + 1);
	req_length += sizeof (char*) * (num_addrs + 1);
	req_length += strlen (h->h_name) + 1;

	if (buflen < req_length) {
		*herr = ERANGE;
		G_UNLOCK (gethost_mutex);
		return -1;
	}

	/* we store the alias/addr pointers in the buffer - figure out
           their addresses here. */
	p = buf;
	if (num_aliases) {
		host->h_aliases = (char**)p;
		p += sizeof (char*) * (num_aliases + 1);
	}
	else
		host->h_aliases = NULL;
	if (num_addrs) {
		host->h_addr_list = (char**)p;
		p += sizeof(char*) * (num_addrs + 1);
	}
	else
		host->h_addr_list = NULL;

	/* copy the host name into the buffer */
	host->h_name = p;
	strcpy (p, h->h_name);
	p += strlen (h->h_name) + 1;
	host->h_addrtype = h->h_addrtype;
	host->h_length = h->h_length;

	/* copy the aliases/addresses into the buffer, and assign the
	   pointers into the hostent */
	*p = 0;
	if (num_aliases) {
		for (i = 0; i < num_aliases; i ++) {
			strcpy (p, h->h_aliases[i]);
			host->h_aliases[i] = p;
			p += strlen (h->h_aliases[i]);
		}
		host->h_aliases[num_aliases] = NULL;
	}

	if (num_addrs) {
		for (i = 0; i < num_addrs; i ++) {
			memcpy (p, h->h_addr_list[i], h->h_length);
			host->h_addr_list[i] = p;
			p += h->h_length;
		}
		host->h_addr_list[num_addrs] = NULL;
	}

	*herr = h_errno;

	G_UNLOCK (gethost_mutex);

	return 0;
#endif
}
