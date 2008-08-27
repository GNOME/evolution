/*
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <libedataserver/e-categories.h>
#include <libecal/e-cal.h>
#include <pi-appinfo.h>

#define PILOT_MAX_CATEGORIES 16


int e_pilot_add_category_if_possible(char *cat_to_add, struct CategoryAppInfo *category);
void e_pilot_local_category_to_remote(int * pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category);
void e_pilot_remote_category_to_local(int   pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category);

