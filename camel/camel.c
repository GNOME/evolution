/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_NSS
#include <nspr.h>
#include <prthread.h>
#include "nss.h"      /* Don't use <> here or it will include the system nss.h instead */
#include <ssl.h>
#endif /* HAVE_NSS */

#include "camel.h"

gboolean camel_verbose_debug = FALSE;

#ifdef HAVE_NSS
static void
camel_shutdown (void)
{
	NSS_Shutdown ();
	
	PR_Cleanup ();
}
#endif /* HAVE_NSS */

gint
camel_init (const char *configdir, gboolean nss_init)
{
#ifdef ENABLE_THREADS
#ifdef G_THREADS_ENABLED	
	/*g_thread_init (NULL);*/
#else /* G_THREADS_ENABLED */
	g_warning ("Threads are not supported by your version of glib");
#endif /* G_THREADS_ENABLED */
#endif /* ENABLE_THREADS */
	
	if (getenv ("CAMEL_VERBOSE_DEBUG"))
		camel_verbose_debug = TRUE;
	
#ifdef HAVE_NSS
	if (nss_init) {
		PR_Init (PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 10);
		
		if (NSS_InitReadWrite (configdir) == SECFailure) {
			/* fall back on using volatile dbs? */
			if (NSS_NoDB_Init (configdir) == SECFailure) {
				g_warning ("Failed to initialize NSS");
				return -1;
			}
		}
		
		NSS_SetDomesticPolicy ();
		
		g_atexit (camel_shutdown);
	}
	
	SSL_OptionSetDefault (SSL_ENABLE_SSL2, PR_TRUE);
	SSL_OptionSetDefault (SSL_ENABLE_SSL3, PR_TRUE);
	SSL_OptionSetDefault (SSL_ENABLE_TLS, PR_TRUE);
	SSL_OptionSetDefault (SSL_V2_COMPATIBLE_HELLO, PR_TRUE /* maybe? */);
#endif /* HAVE_NSS */
	
	return 0;
}
