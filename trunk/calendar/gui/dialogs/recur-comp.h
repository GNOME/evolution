/* Evolution calendar - Recurring calendar component dialog
 *
 * Copyright (C) 2002 Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef RECUR_COMP_H
#define RECUR_COMP_H

#include <gtk/gtkwindow.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-component.h>
#include <libecal/e-cal-util.h>

gboolean recur_component_dialog (ECal *client,
				 ECalComponent *comp,
				 CalObjModType *mod,
				 GtkWindow *parent,
				 gboolean delegated);

#endif
