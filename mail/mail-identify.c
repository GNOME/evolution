/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Dan Winship <danw@helixcode.com>
 *
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com)
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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <libgnome/libgnome.h>
#include "mail.h"

/**
 * mail_identify_mime_part:
 * @part: a CamelMimePart
 *
 * Try to identify the MIME type of the data in @part (which presumably
 * doesn't have a useful Content-Type).
 **/
char *
mail_identify_mime_part (CamelMimePart *part)
{
	GMimeContentField *content_type;
	const char *filename, *type;

	content_type = camel_mime_part_get_content_type (part);


	/* Try identifying based on name in Content-Type or
	 * filename in Content-Disposition.
	 */
	filename = gmime_content_field_get_parameter (content_type, "name");
	if (filename) {
		type = gnome_mime_type_or_default (filename, NULL);
		if (type)
			return g_strdup (type);
	}

	filename = camel_mime_part_get_filename (part);
	if (filename) {
		type = gnome_mime_type_or_default (filename, NULL);
		if (type)
			return g_strdup (type);
	}


	/* Try file magic. */
	/* FIXME */


	/* Another possibility to try is the x-mac-type / x-mac-creator
	 * parameter to Content-Type used by some Mac email clients. That
	 * would require a Mac type to mime type conversion table.
	 */


	/* We give up. */
	return NULL;
}
