/* Event layout engine for Gnomecal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef LAYOUT_H
#define LAYOUT_H

#include "calendar.h"


/* This is the main layout function for overlapping events.  You pass in a list of CalendarObject
 * structures and it will calculate a nice non-overlapping layout for them.
 *
 * It returns the number of slots ("columns") that you need to take into account when actually
 * painting the events, the array of the first slot index that each event occupies, and the array of
 * number of slots that each event occupies.  You have to free both arrays.
 */
void layout_events (GList *events, int *num_slots, int **allocations, int **slots);


#endif
