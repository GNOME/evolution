/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-host-utils.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 * Author: Chris Toshok, Jeffrey Stedfast
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "e-host-utils.h"


#if !defined (HAVE_GETHOSTBYNAME_R) || !defined (HAVE_GETHOSTBYADDR_R)
G_LOCK_DEFINE_STATIC (gethost_mutex);
#endif


#define ALIGN(x) (((x) + (sizeof (char *) - 1)) & ~(sizeof (char *) - 1))

#define GETHOST_PROCESS(h, host, buf, buflen, herr) G_STMT_START {     \
	int num_aliases = 0, num_addrs = 0;                            \
	int req_length;                                                \
	char *p;                                                       \
	int i;                                                         \
	                                                               \
	/* check to make sure we have enough room in our buffer */     \
	req_length = 0;                                                \
	if (h->h_aliases) {                                            \
		for (i = 0; h->h_aliases[i]; i++)                      \
			req_length += strlen (h->h_aliases[i]) + 1;    \
		num_aliases = i;                                       \
	}                                                              \
	                                                               \
	if (h->h_addr_list) {                                          \
		for (i = 0; h->h_addr_list[i]; i++)                    \
			req_length += h->h_length;                     \
		num_addrs = i;                                         \
	}                                                              \
	                                                               \
	req_length += sizeof (char *) * (num_aliases + 1);             \
	req_length += sizeof (char *) * (num_addrs + 1);               \
	req_length += strlen (h->h_name) + 1;                          \
	                                                               \
	if (buflen < req_length) {                                     \
		*herr = ERANGE;                                        \
		G_UNLOCK (gethost_mutex);                              \
		return ERANGE;                                         \
	}                                                              \
	                                                               \
	/* we store the alias/addr pointers in the buffer */           \
        /* their addresses here. */                                    \
	p = buf;                                                       \
	if (num_aliases) {                                             \
		host->h_aliases = (char **) p;                         \
		p += sizeof (char *) * (num_aliases + 1);              \
	} else                                                         \
		host->h_aliases = NULL;                                \
                                                                       \
	if (num_addrs) {                                               \
		host->h_addr_list = (char **) p;                       \
		p += sizeof (char *) * (num_addrs + 1);                \
	} else                                                         \
		host->h_addr_list = NULL;                              \
	                                                               \
	/* copy the host name into the buffer */                       \
	host->h_name = p;                                              \
	strcpy (p, h->h_name);                                         \
	p += strlen (h->h_name) + 1;                                   \
	host->h_addrtype = h->h_addrtype;                              \
	host->h_length = h->h_length;                                  \
	                                                               \
	/* copy the aliases/addresses into the buffer */               \
        /* and assign pointers into the hostent */                     \
	*p = 0;                                                        \
	if (num_aliases) {                                             \
		for (i = 0; i < num_aliases; i++) {                    \
			strcpy (p, h->h_aliases[i]);                   \
			host->h_aliases[i] = p;                        \
			p += strlen (h->h_aliases[i]);                 \
		}                                                      \
		host->h_aliases[num_aliases] = NULL;                   \
	}                                                              \
	                                                               \
	if (num_addrs) {                                               \
		for (i = 0; i < num_addrs; i++) {                      \
			memcpy (p, h->h_addr_list[i], h->h_length);    \
			host->h_addr_list[i] = p;                      \
			p += h->h_length;                              \
		}                                                      \
		host->h_addr_list[num_addrs] = NULL;                   \
	}                                                              \
} G_STMT_END


#ifdef ENABLE_IPv6
/* some helpful utils for IPv6 lookups */
#define IPv6_BUFLEN_MIN  (sizeof (char *) * 3)

static int
ai_to_herr (int error)
{
	switch (error) {
	case EAI_NONAME:
	case EAI_FAIL:
		return HOST_NOT_FOUND;
		break;
	case EAI_SERVICE:
		return NO_DATA;
		break;
	case EAI_ADDRFAMILY:
		return NO_ADDRESS;
		break;
	case EAI_NODATA:
		return NO_DATA;
		break;
	case EAI_MEMORY:
		return ENOMEM;
		break;
	case EAI_AGAIN:
		return TRY_AGAIN;
		break;
	case EAI_SYSTEM:
		return errno;
		break;
	default:
		return NO_RECOVERY;
		break;
	}
}

#endif /* ENABLE_IPv6 */

/**
 * e_gethostbyname_r:
 * @name: the host to resolve
 * @host: a buffer pointing to a struct hostent to use for storage
 * @buf: a buffer to use for hostname storage
 * @buflen: the size of @buf
 * @herr: a pointer to a variable to store an error code in
 *
 * Resolves the hostname @name, in a hopefully-reentrant fashion.
 *
 * Return value: 0 on success, ERANGE if @buflen is too small,
 * "something else" otherwise (in which case *@herr will be set to
 * one of the gethostbyname() error codes).
 **/
int
e_gethostbyname_r (const char *name, struct hostent *host,
		   char *buf, size_t buflen, int *herr)
{
#ifdef ENABLE_IPv6
	struct addrinfo hints, *res;
	int retval, len;
	char *addr;
	
	memset (&hints, 0, sizeof (struct addrinfo));
#ifdef HAVE_AI_ADDRCONFIG
	hints.ai_flags = AI_CANONNAME | AI_ADDRCONFIG;
#else
	hints.ai_flags = AI_CANONNAME;
#endif
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	if ((retval = getaddrinfo (name, NULL, &hints, &res)) != 0) {
		*herr = ai_to_herr (retval);
		return -1;
	}
	
	len = ALIGN (strlen (res->ai_canonname) + 1);
	if (buflen < IPv6_BUFLEN_MIN + len + res->ai_addrlen + sizeof (char *))
		return ERANGE;
	
	/* h_name */
	strcpy (buf, res->ai_canonname);
	host->h_name = buf;
	buf += len;
	
	/* h_aliases */
	((char **) buf)[0] = NULL;
	host->h_aliases = (char **) buf;
	buf += sizeof (char *);
	
	/* h_addrtype and h_length */
	host->h_length = res->ai_addrlen;
	if (res->ai_family == PF_INET6) {
		host->h_addrtype = AF_INET6;
		
		addr = (char *) &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
	} else {
		host->h_addrtype = AF_INET;
		
		addr = (char *) &((struct sockaddr_in *) res->ai_addr)->sin_addr;
	}
	
	memcpy (buf, addr, host->h_length);
	addr = buf;
	buf += ALIGN (host->h_length);
	
	/* h_addr_list */
	((char **) buf)[0] = addr;
	((char **) buf)[1] = NULL;
	host->h_addr_list = (char **) buf;
	
	freeaddrinfo (res);
	
	return 0;
#else /* No support for IPv6 addresses */
#ifdef HAVE_GETHOSTBYNAME_R
#ifdef GETHOSTBYNAME_R_FIVE_ARGS
	if (gethostbyname_r (name, host, buf, buflen, herr))
		return 0;
	else
		return errno;
#else
	struct hostent *hp;
	int retval;
	
	retval = gethostbyname_r (name, host, buf, buflen, &hp, herr);
	if (hp != NULL) {
		*herr = 0;
	} else if (retval == 0) {
		/* glibc 2.3.2 workaround - it seems that
		 * gethostbyname_r will sometimes return 0 on fail and
		 * not set the hostent values (hence the crash in bug
		 * #56337).  Hopefully we can depend on @hp being NULL
		 * in this error case like we do with
		 * gethostbyaddr_r().
		 */
		retval = -1;
	}
	
	return retval;
#endif
#else /* No support for gethostbyname_r */
	struct hostent *h;
	
	G_LOCK (gethost_mutex);
	
	h = gethostbyname (name);
	
	if (!h) {
		*herr = h_errno;
		G_UNLOCK (gethost_mutex);
		return -1;
	}
	
	GETHOST_PROCESS (h, host, buf, buflen, herr);
	
	G_UNLOCK (gethost_mutex);
	
	return 0;
#endif /* HAVE_GETHOSTBYNAME_R */
#endif /* ENABLE_IPv6 */
}


/**
 * e_gethostbyaddr_r:
 * @addr: the addr to resolve
 * @addrlen: address length
 * @type: AF type
 * @host: a buffer pointing to a struct hostent to use for storage
 * @buf: a buffer to use for hostname storage
 * @buflen: the size of @buf
 * @herr: a pointer to a variable to store an error code in
 *
 * Resolves the address @addr, in a hopefully-reentrant fashion.
 *
 * Return value: 0 on success, ERANGE if @buflen is too small,
 * "something else" otherwise (in which case *@herr will be set to
 * one of the gethostbyaddr() error codes).
 **/
int
e_gethostbyaddr_r (const char *addr, int addrlen, int type, struct hostent *host,
		   char *buf, size_t buflen, int *herr)
{
#ifdef ENABLE_IPv6
	int retval, len;
	
	if ((retval = getnameinfo (addr, addrlen, buf, buflen, NULL, 0, NI_NAMEREQD)) != 0) {
		*herr = ai_to_herr (retval);
		return -1;
	}
	
	len = ALIGN (strlen (buf) + 1);
	if (buflen < IPv6_BUFLEN_MIN + len + addrlen + sizeof (char *))
		return ERANGE;
	
	/* h_name */
	host->h_name = buf;
	buf += len;
	
	/* h_aliases */
	((char **) buf)[0] = NULL;
	host->h_aliases = (char **) buf;
	buf += sizeof (char *);
	
	/* h_addrtype and h_length */
	host->h_length = addrlen;
	host->h_addrtype = type;
	
	memcpy (buf, addr, host->h_length);
	addr = buf;
	buf += ALIGN (host->h_length);
	
	/* h_addr_list */
	((char **) buf)[0] = addr;
	((char **) buf)[1] = NULL;
	host->h_addr_list = (char **) buf;
	
	return 0;
#else /* No support for IPv6 addresses */
#ifdef HAVE_GETHOSTBYADDR_R
#ifdef GETHOSTBYADDR_R_SEVEN_ARGS
	if (gethostbyaddr_r (addr, addrlen, type, host, buf, buflen, herr))
		return 0;
	else
		return errno;
#else
	struct hostent *hp;
	int retval;
	
	retval = gethostbyaddr_r (addr, addrlen, type, host, buf, buflen, &hp, herr);
	if (hp != NULL) {
		*herr = 0;
		retval = 0;
	} else if (retval == 0) {
		/* glibc 2.3.2 workaround - it seems that
		 * gethostbyaddr_r will sometimes return 0 on fail and
		 * fill @host with garbage strings from /etc/hosts
		 * (failure to parse the file? who knows). Luckily, it
		 * seems that we can rely on @hp being NULL on
		 * fail.
		 */
		retval = -1;
	}
	
	return retval;
#endif
#else /* No support for gethostbyaddr_r */
	struct hostent *h;
	
	G_LOCK (gethost_mutex);
	
	h = gethostbyaddr (addr, addrlen, type);
	
	if (!h) {
		*herr = h_errno;
		G_UNLOCK (gethost_mutex);
		return -1;
	}
	
	GETHOST_PROCESS (h, host, buf, buflen, herr);
	
	G_UNLOCK (gethost_mutex);
	
	return 0;
#endif /* HAVE_GETHOSTBYADDR_R */
#endif /* ENABLE_IPv6 */
}
