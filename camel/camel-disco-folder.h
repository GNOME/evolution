/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * camel-disco-folder.h: Abstract class for a disconnectable folder
 *
 * Authors: Dan Winship <danw@ximian.com>
 *
 * Copyright 2001 Ximian, Inc.
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

#ifndef CAMEL_DISCO_FOLDER_H
#define CAMEL_DISCO_FOLDER_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include "camel-folder.h"

#define CAMEL_DISCO_FOLDER_TYPE     (camel_disco_folder_get_type ())
#define CAMEL_DISCO_FOLDER(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_DISCO_FOLDER_TYPE, CamelDiscoFolder))
#define CAMEL_DISCO_FOLDER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_DISCO_FOLDER_TYPE, CamelDiscoFolderClass))
#define CAMEL_IS_DISCO_FOLDER(o)    (CAMEL_CHECK_TYPE((o), CAMEL_DISCO_FOLDER_TYPE))

struct _CamelDiscoFolder {
	CamelFolder parent_object;

};

typedef struct {
	CamelFolderClass parent_class;

	void   (*refresh_info_online) (CamelFolder *folder, CamelException *ex);

	void   (*sync_online)    (CamelFolder *folder, CamelException *ex);
	void   (*sync_offline)   (CamelFolder *folder, CamelException *ex);

	void   (*expunge_uids_online)    (CamelFolder *folder, GPtrArray *uids,
					  CamelException *ex);
	void   (*expunge_uids_offline)   (CamelFolder *folder, GPtrArray *uids,
					  CamelException *ex);

	char * (*append_online)    (CamelFolder *folder,
				    CamelMimeMessage *message,
				    const CamelMessageInfo *info,
				    CamelException *ex);
	char * (*append_offline)   (CamelFolder *folder,
				    CamelMimeMessage *message,
				    const CamelMessageInfo *info,
				    CamelException *ex);

	void (*copy_online)    (CamelFolder *source, GPtrArray *uids,
				CamelFolder *destination, CamelException *ex);
	void (*copy_offline)   (CamelFolder *source, GPtrArray *uids,
				CamelFolder *destination, CamelException *ex);
	
	void (*move_online)    (CamelFolder *source, GPtrArray *uids,
				CamelFolder *destination, CamelException *ex);
	void (*move_offline)   (CamelFolder *source, GPtrArray *uids,
				CamelFolder *destination, CamelException *ex);

	void (*update_uid) (CamelFolder *folder, const char *old_uid,
			    const char *new_uid);
} CamelDiscoFolderClass;


/* public methods */
void camel_disco_folder_expunge_uids (CamelFolder *folder, GPtrArray *uids,
				      CamelException *ex);


/* Standard Camel function */
CamelType camel_disco_folder_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_DISCO_FOLDER_H */
