/*
 * Copyright 2008, Joergen Scheibengruber <joergen.scheibengruber@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef __GOOGLE_CONTACTS_SOURCE_H__
#define __GOOGLE_CONTACTS_SOURCE_H__

GtkWidget *plugin_google_contacts (EPlugin                    *epl,
                                   EConfigHookItemFactoryData *data);

void       ensure_google_contacts_source_group (void);

void       remove_google_contacts_source_group (void);

#endif
