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

#include <camel/camel.h>

#include <filter/e-filter-part.h>
#include <filter/e-filter-rule.h>
#include <mail/em-vfolder-rule.h>
#include <shell/e-shell-view.h>

void vfolder_load_storage(void);
void vfolder_revert(void);

void vfolder_edit (EShellView *shell_view);
void vfolder_edit_rule(const gchar *name);
EFilterPart *vfolder_create_part (const gchar *name);
EFilterRule *vfolder_clone_rule (EFilterRule *in);
void vfolder_gui_add_rule (EMVFolderRule *rule);
void vfolder_gui_add_from_message (CamelMimeMessage *msg, gint flags, const gchar *source);
void vfolder_gui_add_from_address (CamelInternetAddress *addr, gint flags, const gchar *source);

GList * mail_vfolder_get_sources_local (void);
GList * mail_vfolder_get_sources_remote (void);

/* close up, clean up */
void mail_vfolder_shutdown (void);

#endif
