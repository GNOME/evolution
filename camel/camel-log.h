/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef CAMEL_LOG_H
#define CAMEL_LOG_H


#include <stdarg.h>
#include <glib.h>
#include <stdio.h>

extern int camel_debug_level;
extern FILE *camel_log_file_descriptor;

typedef enum {
	NO_LOG     =     0,
	STRANGE    =     5,
	WARNING    =     7,
	FULL_DEBUG =     10
} CamelLogLevel;

#define HARD_LOG_LEVEL FULL_DEBUG

/* the idea here is to be able to have a hard maximum log 
level, given at compilation time, and a soft one, given at 
runtime (with camel_debug_level). For the moment, only 
soft level is implmented, but one day, when performance 
become important, I will set the hard one too */

#define CAMEL_LOG(level, args...) camel_log(level,##args)

extern void camel_log(CamelLogLevel level, const gchar *format, ... );

#endif /* CAMEL_LOG_H */
