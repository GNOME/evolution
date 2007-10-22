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

#ifndef EXCHANGE_MAPI_FOLDER_H
#define EXCHANGE_MAPI_FOLDER_H


typedef enum  {
	MAPI_FOLDER_TYPE_MAIL=1,
	MAPI_FOLDER_TYPE_APPOINTMENT,
	MAPI_FOLDER_TYPE_CONTACT,
	MAPI_FOLDER_TYPE_JOURNAL,
	MAPI_FOLDER_TYPE_TASK
} ExchangeMAPIFolderType;

typedef struct _ExchangeMAPIFolder {
	char *folder_name;
	char *parent_folder_name;
	ExchangeMAPIFolderType container_class;
	uint64_t folder_id;
	uint64_t parent_folder_id;
	uint32_t child_count;
} ExchangeMAPIFolder;

ExchangeMAPIFolder *
exchange_mapi_folder_new (const char *folder_name, const char *parent_folder_name, const char *container_class,
			  uint64_t folder_id, uint64_t parent_folder_id, uint32_t child_count);

#endif
