/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Evolution calendar recurrence rule functions
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Damon Chaplin <damon@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef CAL_RECUR_H
#define CAL_RECUR_H

#include <libgnome/gnome-defs.h>
#include <glib.h>
#include <cal-util/cal-component.h>

BEGIN_GNOME_DECLS

typedef gboolean (* CalRecurInstanceFn) (CalComponent *comp,
					 time_t        instance_start,
					 time_t        instace_end,
					 gpointer      data);

/*
 * Calls the given callback function for each occurrence of the event between
 * the given start and end times. If end is 0 it continues until the event
 * ends or forever if the event has an infinite recurrence rule.
 * If the callback routine return FALSE the occurrence generation stops.
 */
void	cal_recur_generate_instances	(CalComponent		*comp,
					 time_t			 start,
					 time_t			 end,
					 CalRecurInstanceFn	 cb,
					 gpointer                cb_data);

END_GNOME_DECLS

#endif
