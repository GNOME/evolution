/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-resp-codes.h : #defines for all the response codes we care about */

/* 
 *
 * Copyright (C) 2000 Helix Code, Inc. <toshok@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifndef CAMEL_NNTP_RESP_CODES_H
#define CAMEL_NNTP_RESP_CODES_H 1

#define CAMEL_NNTP_OK(x) ((x) < 400)
#define CAMEL_NNTP_ERR(x) (!CAMEL_NNTP_OK(x) && (x) < 500)
#define CAMEL_NNTP_FAIL(x) (!CAMEL_NNTP_OK(x) && !CAMEL_NNTP_ERR(x))

#define NNTP_GREETING_POSTING_OK    200
#define NNTP_GREETING_NO_POSTING    201

#define NNTP_GROUP_SELECTED           211
#define NNTP_LIST_FOLLOWS             215
#define NNTP_ARTICLE_FOLLOWS          220
#define NNTP_HEAD_FOLLOWS             221
#define NNTP_DATA_FOLLOWS             224
#define NNTP_NEW_ARTICLE_LIST_FOLLOWS 230
#define NNTP_NEW_GROUP_LIST_FOLLOWS   231

#define NNTP_NO_SUCH_GROUP          411
#define NNTP_NO_SUCH_ARTICLE        430

/* authentication */
#define NNTP_AUTH_ACCEPTED          281
#define NNTP_AUTH_CONTINUE          381
#define NNTP_AUTH_REQUIRED          480
#define NNTP_AUTH_REJECTED          482

#define NNTP_PROTOCOL_ERROR         666

#endif /* CAMEL_NNTP_RESP_CODES_H */
