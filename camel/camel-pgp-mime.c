/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camel-pgp-mime.h"
#include "camel-mime-message.h"


/**
 * camel_pgp_mime_is_rfc2015_signed:
 * @mime_part: MIME part
 *
 * Checks that this part is a multipart/signed part following the
 * rfc2015 and/or rfc3156 PGP/MIME specifications.
 *
 * Returns %TRUE if this looks to be a valid PGP/MIME multipart/signed
 * part or %FALSE otherwise.
 **/
gboolean
camel_pgp_mime_is_rfc2015_signed (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelContentType *type;
#ifdef ENABLE_PEDANTIC_PGPMIME
	const char *param, *micalg;
#endif
	int nparts;
	
	/* check that we have a multipart/signed */
	type = camel_mime_part_get_content_type (mime_part);
	if (!header_content_type_is (type, "multipart", "signed"))
		return FALSE;
	
#ifdef ENABLE_PEDANTIC_PGPMIME
	/* check that we have a protocol param with the value: "application/pgp-signature" */
	param = header_content_type_param (type, "protocol");
	if (!param || strcasecmp (param, "application/pgp-signature"))
		return FALSE;
	
	/* check that we have a micalg parameter */
	micalg = header_content_type_param (type, "micalg");
	if (!micalg)
		return FALSE;
#endif /* ENABLE_PEDANTIC_PGPMIME */
	
	/* check that we have exactly 2 subparts */
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);
	if (nparts != 2)
		return FALSE;
	
	/* The first part may be of any type except for 
	 * application/pgp-signature - check it. */
	part = camel_multipart_get_part (mp, 0);
	type = camel_mime_part_get_content_type (part);
	if (header_content_type_is (type, "application", "pgp-signature"))
		return FALSE;
	
	/* The second part should be application/pgp-signature. */
	part = camel_multipart_get_part (mp, 1);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application", "pgp-signature"))
		return FALSE;
	
	return TRUE;
}


/**
 * camel_pgp_mime_is_rfc2015_encrypted:
 * @mime_part: MIME part
 *
 * Checks that this part is a multipart/encrypted part following the
 * rfc2015 and/or rfc3156 PGP/MIME specifications.
 *
 * Returns %TRUE if this looks to be a valid PGP/MIME
 * multipart/encrypted part or %FALSE otherwise.
 **/
gboolean
camel_pgp_mime_is_rfc2015_encrypted (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelContentType *type;
#ifdef ENABLE_PEDANTIC_PGPMIME
	const char *param;
#endif
	int nparts;
	
	/* check that we have a multipart/encrypted */
	type = camel_mime_part_get_content_type (mime_part);
	if (!header_content_type_is (type, "multipart", "encrypted"))
		return FALSE;
	
#ifdef ENABLE_PEDANTIC_PGPMIME
	/* check that we have a protocol param with the value: "application/pgp-encrypted" */
	param = header_content_type_param (type, "protocol");
	if (!param || strcasecmp (param, "application/pgp-encrypted"))
		return FALSE;
#endif /* ENABLE_PEDANTIC_PGPMIME */
	
	/* check that we have at least 2 subparts */
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);
	if (nparts < 2)
		return FALSE;
	
	/* The first part should be application/pgp-encrypted */
	part = camel_multipart_get_part (mp, 0);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application", "pgp-encrypted"))
		return FALSE;
	
	/* The second part should be application/octet-stream - this
           is the one we care most about */
	part = camel_multipart_get_part (mp, 1);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application", "octet-stream"))
		return FALSE;
	
	return TRUE;
}
