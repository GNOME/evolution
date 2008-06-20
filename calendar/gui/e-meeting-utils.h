/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* itip-attendee.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: JP Rosevear
 */

#ifndef _E_MEETING_UTILS_H_
#define _E_MEETING_UTILS_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include "e-meeting-types.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */



gint e_meeting_time_compare_times (EMeetingTime *time1,
				   EMeetingTime *time2);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_MEETING_UTILS_H_ */


