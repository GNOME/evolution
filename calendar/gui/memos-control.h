/*
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
 * Authors:
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _MEMOS_CONTROL_H_
#define _MEMOS_CONTROL_H_

#include "e-memos.h"

BonoboControl *memos_control_new                (void);
void           memos_control_activate           (BonoboControl *control, EMemos *memos);
void           memos_control_deactivate         (BonoboControl *control, EMemos *memos);
void           memos_control_sensitize_commands (BonoboControl *control, EMemos *memos, gint n_selected);

#endif /* _MEMOS_CONTROL_H_ */
