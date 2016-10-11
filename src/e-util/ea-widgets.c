/*
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
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "ea-factory.h"
#include "ea-calendar-item.h"
#include "ea-widgets.h"

EA_FACTORY_GOBJECT (EA_TYPE_CALENDAR_ITEM, ea_calendar_item, ea_calendar_item_new)

void e_calendar_item_a11y_init (void)
{
    EA_SET_FACTORY (e_calendar_item_get_type (), ea_calendar_item);
}
