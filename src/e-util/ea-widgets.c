/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Bolian Yin <bolian.yin@sun.com>
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
