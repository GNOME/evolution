/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Leon Zhang <leon.zhang@sun.com>
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
