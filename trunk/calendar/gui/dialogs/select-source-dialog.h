/* Evolution calendar - Select source dialog
 *
 * Copyright (C) 2004 Novell, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef SELECT_SOURCE_DIALOG_H
#define SELECT_SOURCE_DIALOG_H

#include <gtk/gtkwindow.h>
#include <libedataserver/e-source.h>
#include <libecal/e-cal.h>

ESource *select_source_dialog (GtkWindow *parent, ECalSourceType type);

#endif
