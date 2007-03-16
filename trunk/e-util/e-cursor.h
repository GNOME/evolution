/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 *  Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 *  Copyright 2004 Novell Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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

#ifndef _E_CURSOR_H
#define _E_CURSOR_H

#include <gtk/gtk.h>

typedef enum {

	E_CURSOR_NORMAL,
	E_CURSOR_BUSY,

}ECursorType;

void e_cursor_set (GtkWidget *widget, ECursorType cursor);

#endif /* !_E_CURSOR_H */
