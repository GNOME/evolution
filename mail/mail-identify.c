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
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-sniff-buffer.h>
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
	GnomeVFSMimeSniffBuffer *sniffer;
	CamelStream *memstream;
	CamelDataWrapper *data;
	GByteArray *ba;

	content_type = camel_mime_part_get_content_type (part);


	/* Try identifying based on name in Content-Type or
	 * filename in Content-Disposition.
	 */
	filename = gmime_content_field_get_parameter (content_type, "name");
	if (filename) {
		type = gnome_vfs_mime_type_from_name_or_default (filename,
								 NULL);
		if (type)
			return g_strdup (type);
	}

	filename = camel_mime_part_get_filename (part);
	if (filename) {
		type = gnome_vfs_mime_type_from_name_or_default (filename,
								 NULL);
		if (type)
			return g_strdup (type);
	}


	/* Try file magic. */
	data = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	ba = g_byte_array_new ();
	memstream = camel_stream_mem_new_with_byte_array (ba);
	camel_data_wrapper_write_to_stream (data, memstream);
	if (ba->len) {
		sniffer = gnome_vfs_mime_sniff_buffer_new_from_memory (
			ba->data, ba->len);
		type = gnome_vfs_get_mime_type_for_buffer (sniffer);
		gnome_vfs_mime_sniff_buffer_free (sniffer);
	} else
		type = NULL;
	gtk_object_unref (GTK_OBJECT (memstream));

	if (type)
		return g_strdup (type);


	/* Another possibility to try is the x-mac-type / x-mac-creator
	 * parameter to Content-Type used by some Mac email clients. That
	 * would require a Mac type to mime type conversion table.
	 */


	/* We give up. */
	return NULL;
}
