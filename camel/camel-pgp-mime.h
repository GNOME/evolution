/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
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


#ifndef CAMEL_PGP_MIME_H
#define CAMEL_PGP_MIME_H

#include <glib.h>
#include <camel/camel-multipart.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-pgp-context.h>
#include <camel/camel-exception.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

gboolean camel_pgp_mime_is_rfc2015_signed (CamelMimePart *part);
gboolean camel_pgp_mime_is_rfc2015_encrypted (CamelMimePart *part);

void camel_pgp_mime_part_sign (CamelPgpContext *context,
			       CamelMimePart **mime_part,
			       const char *userid,
			       CamelCipherHash hash,
			       CamelException *ex);

CamelCipherValidity *camel_pgp_mime_part_verify (CamelPgpContext *context,
						 CamelMimePart *mime_part,
						 CamelException *ex);

void camel_pgp_mime_part_encrypt (CamelPgpContext *context,
				  CamelMimePart **mime_part,
				  GPtrArray *recipients,
				  CamelException *ex);

CamelMimePart *camel_pgp_mime_part_decrypt (CamelPgpContext *context,
					    CamelMimePart *mime_part,
					    CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! CAMEL_PGP_MIME_H */
