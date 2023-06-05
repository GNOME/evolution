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
 *		Leon Zhang <leon.zhang@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-util/e-util.h"

#include "ea-addressbook.h"
#include "ea-addressbook-view.h"

EA_FACTORY_GOBJECT (EA_TYPE_AB_VIEW, ea_ab_view, ea_ab_view_new)

void eab_view_a11y_init (void)
{
	EA_SET_FACTORY (E_TYPE_ADDRESSBOOK_VIEW, ea_ab_view);
}
