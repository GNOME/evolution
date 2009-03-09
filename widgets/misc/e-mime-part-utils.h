/*
 * e-mime-part-utils.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MIME_PART_UTILS_H
#define E_MIME_PART_UTILS_H

#include <gtk/gtk.h>
#include <camel/camel-mime-part.h>
#include <shell/e-shell-window.h>

G_BEGIN_DECLS

GList *		e_mime_part_utils_get_apps	(CamelMimePart *mime_part);
gboolean	e_mime_part_utils_save_to_file	(CamelMimePart *mime_part,
						 GFile *file,
						 GError **error);

void		e_mime_part_utils_add_open_actions
						(CamelMimePart *mime_part,
						 GtkUIManager *ui_manager,
						 GtkActionGroup *action_group,
						 const gchar *widget_path,
						 GtkWindow *parent,
						 guint merge_id);

G_END_DECLS

#endif /* E_MIME_PART_UTILS_H */
