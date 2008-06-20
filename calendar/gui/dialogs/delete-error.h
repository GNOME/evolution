/* Evolution calendar - Send calendar component dialog
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Author: JP Rosevear <jpr@ximian.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DELETE_ERROR_H
#define DELETE_ERROR_H

#include <glib.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-component.h>

void delete_error_dialog (GError *error, ECalComponentVType vtype);

#endif
