/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar importer component
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef EVOLUTION_CALENDAR_IMPORTER_H
#define EVOLUTION_CALENDAR_IMPORTER_H

#include <bonobo/bonobo-object.h>

BEGIN_GNOME_DECLS

BonoboObject *ical_importer_new (void);
BonoboObject *vcal_importer_new (void);

BonoboObject *gnome_calendar_importer_new (void);

END_GNOME_DECLS

#endif
