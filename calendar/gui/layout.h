/* Evolution calendar - Event layout engine
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Miguel de Icaza <miguel@helixcode.com>
 *          Federico Mena-Quintero <federico@helixcode.com>
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

#ifndef LAYOUT_H
#define LAYOUT_H

#include <glib.h>
#include <time.h>



/* Functions of this type must translate the list item into two time_t values
 * for the start and end of an event.
 */
typedef void (* LayoutQueryTimeFunc) (GList *event, time_t *start, time_t *end);


/* This is the main layout function for overlapping events.  You pass in a list
 * of (presumably) events and a function that should take a list element and
 * return the start and end times for the event corresponding to that list
 * element.
 *
 * It returns the number of slots ("columns") that you need to take into account
 * when actually painting the events, the array of the first slot index that
 * each event occupies, and the array of number of slots that each event
 * occupies.  You have to free both arrays.
 *
 * You will get somewhat better-looking results if the list of events is sorted
 * by using the start time as the primary sort key and the end time as the
 * secondary sort key -- so that "longer" events go first in the list.
 */
void layout_events (GList *events, LayoutQueryTimeFunc func,
		    int *num_slots, int **allocations, int **slots);



#endif
