/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2003, Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifndef _AUTHENTICATION_H_
#define _AUTHENTICATION_H_

#include <libedataserver/e-source.h>
#include <libecal/e-cal.h>

ECal *auth_new_cal_from_default (ECalSourceType type);
ECal *auth_new_cal_from_source (ESource *source, ECalSourceType type);
ECal *auth_new_cal_from_uri (const char *uri, ECalSourceType type);
void auth_cal_forget_password (ECal *ecal);

#endif
