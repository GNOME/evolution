/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
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

#ifndef MAIL_CRYPTO_H
#define MAIL_CRYPTO_H

#include <camel/camel.h>
#include <camel/camel-smime-context.h>
#include <camel/camel-smime-utils.h>
#include "mail-config.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus  */


/* PGP/MIME convenience wrappers */
CamelCipherContext *mail_crypto_get_pgp_cipher_context (EAccount *account);


/* S/MIME v3 convenience wrappers */
CamelMimeMessage *mail_crypto_smime_sign      (CamelMimeMessage *message, const char *userid,
					       gboolean signing_time, gboolean detached,
					       CamelException *ex);

CamelMimeMessage *mail_crypto_smime_certsonly (CamelMimeMessage *message, const char *userid,
					       GPtrArray *recipients, CamelException *ex);

CamelMimeMessage *mail_crypto_smime_encrypt   (CamelMimeMessage *message, const char *userid,
					       GPtrArray *recipients, CamelException *ex);

CamelMimeMessage *mail_crypto_smime_envelope  (CamelMimeMessage *message, const char *userid,
					       GPtrArray *recipients, CamelException *ex);

CamelMimeMessage *mail_crypto_smime_decode    (CamelMimeMessage *message,
					       CamelCMSValidityInfo **info, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! MAIL_CRYPTO_H */
