/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component-utils.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-shell-component-utils.h"
#include <e-util/e-icon-factory.h>
#include "e-util/e-dialog-utils.h"

#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-activation/bonobo-activation.h>

static void free_pixmaps (void);
static GSList *inited_arrays = NULL;

void e_pixmaps_update (BonoboUIComponent *uic, EPixmap *pixcache)
{
	static int done_init = 0;
	int i;

	if (!done_init) {
		g_atexit (free_pixmaps);
		done_init = 1;
	}

	if (g_slist_find (inited_arrays, pixcache) == NULL)
		inited_arrays = g_slist_prepend (inited_arrays, pixcache);

	for (i = 0; pixcache [i].path; i++) {
		if (!pixcache [i].pixbuf) {
			GdkPixbuf *pixbuf;

			pixbuf = e_icon_factory_get_icon (pixcache [i].name, pixcache [i].size);
			pixcache [i].pixbuf = bonobo_ui_util_pixbuf_to_xml (pixbuf);
			g_object_unref (pixbuf);
			bonobo_ui_component_set_prop (uic,
				pixcache [i].path, "pixname",
				pixcache [i].pixbuf, NULL);
		} else {
			bonobo_ui_component_set_prop (uic, pixcache [i].path,
						      "pixname",
						      pixcache [i].pixbuf,
						      NULL);
		}
	}
}

static void
free_pixmaps (void)
{
	int i;
	GSList *li;

	for (li = inited_arrays; li != NULL; li = li->next) {
		EPixmap *pixcache = li->data;
		for (i = 0; pixcache [i].path; i++)
			g_free (pixcache [i].pixbuf);
	}

	g_slist_free (inited_arrays);
}


/**
 * e_get_activation_failure_msg:
 * @ev: An exception returned by an oaf_activate call.
 * 
 * Get a descriptive error message from @ev.
 * 
 * Return value: A newly allocated string with the printable error message.
 **/
char *
e_get_activation_failure_msg (CORBA_Environment *ev)
{
	g_return_val_if_fail (ev != NULL, NULL);

	if (CORBA_exception_id (ev) == NULL)
		return NULL;

	if (strcmp (CORBA_exception_id (ev), ex_Bonobo_GeneralError) != 0) {
		return bonobo_exception_get_text (ev); 
	} else {
		const Bonobo_GeneralError *oaf_general_error;

		oaf_general_error = CORBA_exception_value (ev);
		return g_strdup (oaf_general_error->description);
	}
}
