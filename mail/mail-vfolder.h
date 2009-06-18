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

#include <camel/camel-internet-address.h>
#include <camel/camel-mime-message.h>

#include <filter/filter-part.h>
#include <filter/filter-rule.h>
#include <mail/em-vfolder-rule.h>

void vfolder_load_storage(void);
void vfolder_revert(void);

void vfolder_edit (void);
void vfolder_edit_rule(const gchar *name);
FilterPart *vfolder_create_part (const gchar *name);
FilterRule *vfolder_clone_rule (FilterRule *in);
void vfolder_gui_add_rule (EMVFolderRule *rule);
void vfolder_gui_add_from_message (CamelMimeMessage *msg, gint flags, const gchar *source);
void vfolder_gui_add_from_address (CamelInternetAddress *addr, gint flags, const gchar *source);

GList * mail_vfolder_get_sources_local (void);
GList * mail_vfolder_get_sources_remote (void);

/* add a uri that is now (un)available to vfolders in a transient manner */
void mail_vfolder_add_uri(CamelStore *store, const gchar *uri, gint remove);

/* note that a folder has changed name (uri) */
void mail_vfolder_rename_uri(CamelStore *store, const gchar *from, const gchar *to);

/* remove a uri that should be removed from vfolders permanently */
void mail_vfolder_delete_uri(CamelStore *store, const gchar *uri);

/* close up, clean up */
void mail_vfolder_shutdown (void);

#endif
