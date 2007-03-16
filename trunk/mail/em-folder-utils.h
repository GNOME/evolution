/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *           Rodney Dawes <dobey@novell.com>
 *
 *  Copyright 2003-2005 Novell, Inc. (www.novell.com)
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

#ifndef _EM_FOLDER_UTILS_H
#define _EM_FOLDER_UTILS_H

int em_folder_utils_copy_folders(CamelStore *fromstore, const char *frombase, CamelStore *tostore, const char *tobase, int delete);

/* FIXME: These api's are really busted, there is no consistency and most rely on the wrong data */

void em_folder_utils_copy_folder (struct _CamelFolderInfo *folderinfo, int delete);

void em_folder_utils_delete_folder (struct _CamelFolder *folder);
void em_folder_utils_rename_folder (struct _CamelFolder *folder);

void em_folder_utils_create_folder (struct _CamelFolderInfo *folderinfo);

#endif
