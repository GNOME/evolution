/*
 *
 * Evolution calendar - Copy source dialog
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef COPY_SOURCE_DIALOG_H
#define COPY_SOURCE_DIALOG_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <calendar/gui/e-cal-model.h>

void		copy_source_dialog		(GtkWindow *parent,
						 ECalModel *model,
						 ESource *from_source);

#endif /* COPY_SOURCE_DIALOG_H */
