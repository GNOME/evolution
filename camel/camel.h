/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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


#ifndef CAMEL_H
#define CAMEL_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-data-wrapper.h>
#include <camel/camel-exception.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-filter-basic.h>
#include <camel/camel-mime-filter-bestenc.h>
#include <camel/camel-mime-filter-canon.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-mime-filter-crlf.h>
#include <camel/camel-mime-filter-enriched.h>
#include <camel/camel-mime-filter-from.h>
#include <camel/camel-mime-filter-gzip.h>
#include <camel/camel-mime-filter-html.h>
#include <camel/camel-mime-filter-index.h>
#include <camel/camel-mime-filter-linewrap.h>
#include <camel/camel-mime-filter-save.h>
#include <camel/camel-mime-filter-tohtml.h>
#include <camel/camel-mime-filter-yenc.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-parser.h>
#include <camel/camel-mime-part-utils.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-multipart.h>
#include <camel/camel-multipart-encrypted.h>
#include <camel/camel-multipart-signed.h>
#include <camel/camel-seekable-stream.h>
#include <camel/camel-seekable-substream.h>
#include <camel/camel-stream-buffer.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream.h>
#include <camel/camel-transport.h>
#include <camel/camel-url.h>
#include <camel/camel-string-utils.h>

#include <glib.h>

int camel_init (const char *certdb_dir, gboolean nss_init);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_H */
