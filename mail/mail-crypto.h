/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifndef MAIL_CRYPTO_H
#define MAIL_CRYPTO_H

#include <gnome.h>
#include <camel/camel.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

char *mail_crypto_openpgp_decrypt (const char *ciphertext,
				   CamelException *ex);

char *mail_crypto_openpgp_encrypt (const char *plaintext,
				   const GPtrArray *recipients,
				   gboolean sign,
				   CamelException *ex);

char *mail_crypto_openpgp_clearsign (const char *plaintext,
				     const char *userid,
				     CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! MAIL_CRYPTO_H */
