/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Various utilities for the mbox provider */

/* 
 * Authors : 
 *   Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright (C) 1999 Helix Code.
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


#ifndef CAMEL_MBOX_UTILS_H
#define CAMEL_MBOX_UTILS_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/


#include "camel-exception.h"
#include "camel-mbox-folder.h"

void 
camel_mbox_xev_parse_header_content (gchar header_content[6], 
				     guint32 *uid, 
				     guchar *status);

void 
camel_mbox_xev_write_header_content (gchar header_content[6], 
				     guint32 uid, 
				     guchar status);

guint32
camel_mbox_write_xev (CamelMboxFolder *folder,
		      gchar *mbox_file_name,
		      GArray *summary_information, 
		      guint32 *file_size,
		      guint32 last_uid, 
		      CamelException *ex);

GArray *
parsed_information_to_mbox_summary (GArray *parsed_information);

void 
camel_mbox_copy_file_chunk (gint fd_src,
			    gint fd_dest, 
			    glong nb_bytes, 
			    CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MBOX_UTILS_H */
