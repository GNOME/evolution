/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Base64 handlers
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 */

#ifndef _GMIME_BASE64_H
#define _GMIME_BASE64_H

#include "camel-stream.h"

void gmime_encode_base64 (CamelStream *input, CamelStream *output);

#endif
