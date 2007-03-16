/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* memos-control.h
 *
 * Copyright (C) 2000, 2001, 2002, 2003  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 */

#ifndef _MEMOS_CONTROL_H_
#define _MEMOS_CONTROL_H_

#include "e-memos.h"

BonoboControl *memos_control_new                (void);
void           memos_control_activate           (BonoboControl *control, EMemos *memos);
void           memos_control_deactivate         (BonoboControl *control, EMemos *memos);
void           memos_control_sensitize_commands (BonoboControl *control, EMemos *memos, int n_selected);

#endif /* _MEMOS_CONTROL_H_ */
