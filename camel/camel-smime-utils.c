/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#include "camel-smime-utils.h"
#include "camel-multipart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define d(x) x

/** rfc2633 stuff (aka S/MIME v3) ********************************/

gboolean
camel_smime_is_smime_v3_signed (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelContentType *type;
	const gchar *param, *micalg;
	int nparts;
	
	/* check that we have a multipart/signed */
	type = camel_mime_part_get_content_type (mime_part);
	if (!header_content_type_is (type, "multipart", "signed"))
		return FALSE;
	
	/* check that we have a protocol param with the value: "application/pkcs7-signature" */
	param = header_content_type_param (type, "protocol");
	if (!param || g_strcasecmp (param, "application/pkcs7-signature"))
		return FALSE;
	
	/* check that we have a micalg parameter */
	micalg = header_content_type_param (type, "micalg");
	if (!micalg)
		return FALSE;
	
	/* check that we have exactly 2 subparts */
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);
	if (nparts != 2)
		return FALSE;
	
	/* The first part may be of any type except for 
	 * application/pkcs7-signature - check it. */
	part = camel_multipart_get_part (mp, 0);
	type = camel_mime_part_get_content_type (part);
	if (header_content_type_is (type, "application", "pkcs7-signature"))
		return FALSE;
	
	/* The second part should be application/pkcs7-signature. */
	part = camel_multipart_get_part (mp, 1);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application", "pkcs7-signature"))
		return FALSE;
	
	return TRUE;
}

gboolean
camel_smime_is_smime_v3_encrypted (CamelMimePart *mime_part)
{
	char *types[] = { "p7m", "p7c", "p7s", NULL };
	const gchar *param, *filename;
	CamelContentType *type;
	int i;
	
	/* check that we have a application/pkcs7-mime part */
	type = camel_mime_part_get_content_type (mime_part);
	if (header_content_type_is (type, "application", "pkcs7-mime")) {
		/* check to make sure it's an encrypted pkcs7-mime part? */
		return TRUE;
	}
	
	if (header_content_type_is (type, "application", "octent-stream")) {
		/* check to see if we have a paremeter called "smime-type" */
		param = header_content_type_param (type, "smime-type");
		if (param)
			return TRUE;
		
		/* check to see if there is a name param and if it has a smime extension */
		param = header_content_type_param (type, "smime-type");
		if (param && *param && strlen (param) > 4) {
			for (i = 0; types[i]; i++)
				if (!g_strcasecmp (param + strlen (param)-4, types[i]))
					return TRUE;
		}
		
		/* check to see if there is a name param and if it has a smime extension */
		filename = camel_mime_part_get_filename (mime_part);
		if (filename && *filename && strlen (filename) > 4) {
			for (i = 0; types[i]; i++)
				if (!g_strcasecmp (filename + strlen (filename)-4, types[i]))
					return TRUE;
		}
	}
	
	return FALSE;
}
