/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-command.h: IMAP command sending/parsing routines */

/* 
 * Authors:
 *   Dan Winship <danw@ximian.com>
 *   Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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


#ifndef CAMEL_IMAP_COMMAND_H
#define CAMEL_IMAP_COMMAND_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include "camel-imap-types.h"

struct _CamelImapResponse {
	CamelFolder *folder;
	GPtrArray *untagged;
	char *status;
};

CamelImapResponse *camel_imap_command              (CamelImapStore *store,
						    CamelFolder *folder,
						    CamelException *ex,
						    const char *fmt, ...);
CamelImapResponse *camel_imap_command_continuation (CamelImapStore *store,
						    CamelException *ex,
						    const char *cmdbuf);

void  camel_imap_response_free                 (CamelImapStore *store,
						CamelImapResponse *response);
void  camel_imap_response_free_without_processing(CamelImapStore *store,
						  CamelImapResponse *response);
char *camel_imap_response_extract              (CamelImapStore *store,
						CamelImapResponse *response,
						const char *type,
						CamelException *ex);
char *camel_imap_response_extract_continuation (CamelImapStore *store,
						CamelImapResponse *response,
						CamelException *ex);

#endif /* CAMEL_IMAP_COMMAND_H */
