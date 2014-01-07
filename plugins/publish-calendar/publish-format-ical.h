/*
 *
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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <libecal/libecal.h>

#include "publish-location.h"

#ifndef PUBLISH_FORMAT_ICAL_H
#define PUBLISH_FORMAT_ICAL_H

void publish_calendar_as_ical (GOutputStream *stream, EPublishUri *uri, GError **error);

#endif
