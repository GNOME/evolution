/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _MAIL_VFOLDER_H
#define _MAIL_VFOLDER_H

struct _CamelStore;
struct _FilterPart;
struct _FilterRule;
struct _CamelMimeMessage;
struct _EMVFolderRule;
struct _CamelInternetAddress;

void vfolder_load_storage(void);
void vfolder_revert(void);

void vfolder_edit (void);
void vfolder_edit_rule(const char *name);
struct _FilterPart *vfolder_create_part (const char *name);
struct _FilterRule *vfolder_clone_rule (struct _FilterRule *in);
void vfolder_gui_add_rule (struct _EMVFolderRule *rule);
void vfolder_gui_add_from_message (struct _CamelMimeMessage *msg, int flags, const char *source);
void vfolder_gui_add_from_address (struct _CamelInternetAddress *addr, int flags, const char *source);

GList * mail_vfolder_get_sources_local (void);
GList * mail_vfolder_get_sources_remote (void);

/* add a uri that is now (un)available to vfolders in a transient manner */
void mail_vfolder_add_uri(struct _CamelStore *store, const char *uri, int remove);

/* note that a folder has changed name (uri) */
void mail_vfolder_rename_uri(struct _CamelStore *store, const char *from, const char *to);

/* remove a uri that should be removed from vfolders permanently */
void mail_vfolder_delete_uri(struct _CamelStore *store, const char *uri);

/* close up, clean up */
void mail_vfolder_shutdown (void);

#endif
