/*
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
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "ea-factory.h"
#include "widgets/ea-calendar-item.h"
#include "widgets/ea-combo-button.h"
#include "ea-widgets.h"

EA_FACTORY_GOBJECT (EA_TYPE_CALENDAR_ITEM, ea_calendar_item, ea_calendar_item_new)
EA_FACTORY (EA_TYPE_COMBO_BUTTON, ea_combo_button, ea_combo_button_new)

void e_calendar_item_a11y_init (void)
{
    EA_SET_FACTORY (e_calendar_item_get_type (), ea_calendar_item);
}

void e_combo_button_a11y_init (void)
{
    EA_SET_FACTORY (e_combo_button_get_type (), ea_combo_button);
}
