/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2004 Novell Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#include <stdlib.h>
#include <string.h>

#include "camel-debug.h"

int camel_verbose_debug;

static GHashTable *debug_table = NULL;

/**
 * camel_debug_init:
 * @void: 
 * 
 * Init camel debug.  Maintain legacy CAMEL_VERBOSE_DEBUG as well as the
 * new CAMEL_DEBUG based environment variable interfaces.
 *
 * CAMEL_VERBOSE_DEBUG is set to a number to turn debug on.
 *
 * CAMEL_DEBUG is set to a comma separated list of modules to debug.
 * The modules can contain module-specific specifiers after a ':', or
 * just act as a wildcard for the module or even specifier.  e.g. 'imap'
 * for imap debug, or 'imap:folder' for imap folder debug.  Additionaly,
 * ':folder' can be used for a wildcard for any folder operations.
 **/
void camel_debug_init(void)
{
	char *d;

	d = getenv("CAMEL_VERBOSE_DEBUG");
	if (d)
		camel_verbose_debug = atoi(d);

	d = g_strdup(getenv("CAMEL_DEBUG"));
	if (d) {
		char *p;

		debug_table = g_hash_table_new(g_str_hash, g_str_equal);
		p = d;
		while (*p) {
			while (*p && *p != ',')
				p++;
			if (*p)
				*p++ = 0;
			g_hash_table_insert(debug_table, d, d);
			d = p;
		}

		if (g_hash_table_lookup(debug_table, "all"))
			camel_verbose_debug = 1;
	}
}

/**
 * camel_debug:
 * @mode: 
 * 
 * Check to see if a debug mode is activated.  @mode takes one of two forms,
 * a fully qualified 'module:target', or a wildcard 'module' name.  It
 * returns a boolean to indicate if the module or module and target is
 * currently activated for debug output.
 * 
 * Return value: 
 **/
gboolean camel_debug(const char *mode)
{
	if (camel_verbose_debug)
		return TRUE;

	if (debug_table) {
		char *colon;
		char *fallback;

		if (g_hash_table_lookup(debug_table, mode))
			return TRUE;

		/* Check for fully qualified debug */
		colon = strchr(mode, ':');
		if (colon) {
			fallback = g_alloca(strlen(mode)+1);
			strcpy(fallback, mode);
			colon = (colon-mode) + fallback;
			/* Now check 'module[:*]' */
			*colon = 0;
			if (g_hash_table_lookup(debug_table, fallback))
				return TRUE;
			/* Now check ':subsystem' */
			*colon = ':';
			if (g_hash_table_lookup(debug_table, colon))
				return TRUE;		
		}
	}

	return FALSE;
}
