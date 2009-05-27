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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright 2008, Joergen Scheibengruber <joergen.scheibengruber@googlemail.com>
 *
 */

#ifndef __GOOGLE_CONTACTS_SOURCE_H__
#define __GOOGLE_CONTACTS_SOURCE_H__

GtkWidget *plugin_google_contacts (EPlugin                    *epl,
                                   EConfigHookItemFactoryData *data);

void       ensure_google_contacts_source_group (void);

void       remove_google_contacts_source_group (void);

#endif
