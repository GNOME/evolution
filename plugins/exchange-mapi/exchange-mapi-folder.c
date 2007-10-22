/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Srinivasa Ragavan <sragavan@novell.com>
 *  Copyright (C) 2007 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <libmapi/libmapi.h>
#include "exchange-mapi-folder.h"

static ExchangeMAPIFolderType
container_class_to_type (const char *type)
{
	ExchangeMAPIFolderType folder_type = MAPI_FOLDER_TYPE_MAIL;

	if (!strcmp (type, IPF_APPOINTMENT)) 
		folder_type = MAPI_FOLDER_TYPE_APPOINTMENT;
	else if (!strcmp (type, IPF_CONTACT))
		folder_type = MAPI_FOLDER_TYPE_CONTACT;
	else if (!strcmp (type, IPF_JOURNAL))
		folder_type = MAPI_FOLDER_TYPE_JOURNAL;
	else if (!strcmp (type, IPF_TASK))
		folder_type = MAPI_FOLDER_TYPE_TASK;

	/* Else it has to be a mail folder only. It is assumed in MAPI code as well. */

	return folder_type;
}

ExchangeMAPIFolder *
exchange_mapi_folder_new (const char *folder_name, const char *parent_folder_name, const char *container_class,
			  uint64_t folder_id, uint64_t parent_folder_id, uint32_t child_count)
{
	ExchangeMAPIFolder *folder;

	folder = g_new (ExchangeMAPIFolder, 1);
	folder->folder_name = g_strdup (folder_name);
	folder->parent_folder_name = parent_folder_name ? g_strdup (parent_folder_name) : NULL;
	folder->container_class = container_class_to_type (container_class);
	folder->folder_id = folder_id;
	folder->parent_folder_id = parent_folder_id;
	folder->child_count = child_count;

	return folder;
}
