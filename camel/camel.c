/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *           Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 *  Copyright 1999-2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <signal.h>

#ifdef HAVE_NSS
#include <nspr.h>
#include <prthread.h>
#include "nss.h"      /* Don't use <> here or it will include the system nss.h instead */
#include <ssl.h>
#endif /* HAVE_NSS */

#include "camel.h"
#include "camel-certdb.h"
#include "camel-mime-utils.h"
#include "camel-provider.h"
#include "camel-debug.h"

static int initialised = FALSE;

static void
camel_shutdown (void)
{
	void camel_operation_shutdown (void);
	CamelCertDB *certdb;
	
	if (!initialised)
		return;
	
#ifdef HAVE_NSS
	NSS_Shutdown ();
	
	PR_Cleanup ();
#endif /* HAVE_NSS */
	
	certdb = camel_certdb_get_default ();
	if (certdb) {
		camel_certdb_save (certdb);
		camel_object_unref (certdb);
	}
	
	camel_operation_shutdown ();
	camel_mime_utils_shutdown ();
	
	initialised = FALSE;
}

int
camel_init (const char *configdir, gboolean nss_init)
{
	CamelCertDB *certdb;
	char *path;
	void camel_operation_init(void);
	
	if (initialised)
		return 0;

	camel_debug_init();

	/* initialise global camel_object_type */
	camel_object_get_type();

	camel_mime_utils_init();
	camel_operation_init();

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
		
		SSL_OptionSetDefault (SSL_ENABLE_SSL2, PR_TRUE);
		SSL_OptionSetDefault (SSL_ENABLE_SSL3, PR_TRUE);
		SSL_OptionSetDefault (SSL_ENABLE_TLS, PR_TRUE);
		SSL_OptionSetDefault (SSL_V2_COMPATIBLE_HELLO, PR_TRUE /* maybe? */);
	}
#endif /* HAVE_NSS */
	
	path = g_strdup_printf ("%s/camel-cert.db", configdir);
	certdb = camel_certdb_new ();
	camel_certdb_set_filename (certdb, path);
	g_free (path);
	
	/* if we fail to load, who cares? it'll just be a volatile certdb */
	camel_certdb_load (certdb);
	
	/* set this certdb as the default db */
	camel_certdb_set_default (certdb);
	
	camel_object_unref (certdb);
	
	g_atexit (camel_shutdown);
	
	initialised = TRUE;
	
	return 0;
}
