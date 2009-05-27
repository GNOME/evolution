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
 * Authors:
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _AUTHENTICATION_H_
#define _AUTHENTICATION_H_

#include <libedataserver/e-source.h>
#include <libecal/e-cal.h>

ECal *auth_new_cal_from_default (ECalSourceType type);
ECal *auth_new_cal_from_source (ESource *source, ECalSourceType type);
ECal *auth_new_cal_from_uri (const gchar *uri, ECalSourceType type);
void auth_cal_forget_password (ECal *ecal);

#endif
