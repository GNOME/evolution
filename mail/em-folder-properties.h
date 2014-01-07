/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_FOLDER_PROPERTIES_H__
#define __EM_FOLDER_PROPERTIES_H__

#include <camel/camel.h>
#include <e-util/e-util.h>

G_BEGIN_DECLS

void		em_folder_properties_show	(CamelStore *store,
						 const gchar *folder_name,
						 EAlertSink *alert_sink,
						 GtkWindow *parent_window);

G_END_DECLS

#endif /* __EM_FOLDER_PROPERTIES_H__ */
