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

#define CAMEL_LOG_LEVEL_NO_LOG 0
#define CAMEL_LOG_LEVEL_STRANGE 5
#define CAMEL_LOG_LEVEL_WARNING 6
#define CAMEL_LOG_LEVEL_TRACE 8
#define CAMEL_LOG_LEVEL_FULL_DEBUG 10

/*  #define CAMEL_HARD_LOG_LEVEL CAMEL_LOG_LEVEL_TRACE */

/* the idea here is to be able to have a hard maximum log 
level, given at compilation time, and a soft one, given at 
runtime (with camel_debug_level) */

#if CAMEL_HARD_LOG_LEVEL>=CAMEL_LOG_LEVEL_STRANGE
#define CAMEL_LOG_STRANGE(args...) camel_log(CAMEL_LOG_LEVEL_STRANGE, ##args)
#else  /* CAMEL_LOG_LEVEL_STRANGE */
#define CAMEL_LOG_STRANGE(args...) 
#endif /* CAMEL_LOG_LEVEL_STRANGE */

#if CAMEL_HARD_LOG_LEVEL>=CAMEL_LOG_LEVEL_WARNING
#define CAMEL_LOG_WARNING(args...) camel_log(CAMEL_LOG_LEVEL_WARNING, ##args)
#else  /* CAMEL_LOG_LEVEL_WARNING */
#define CAMEL_LOG_WARNING(args...) 
#endif /* CAMEL_LOG_LEVEL_WARNING */

#if CAMEL_HARD_LOG_LEVEL>=CAMEL_LOG_LEVEL_TRACE
#define CAMEL_LOG_TRACE(args...) camel_log(CAMEL_LOG_LEVEL_TRACE, ##args)
#else  /* CAMEL_LOG_LEVEL_TRACE */
#define CAMEL_LOG_TRACE(args...) 
#endif /* CAMEL_LOG_LEVEL_TRACE */

#if CAMEL_HARD_LOG_LEVEL>=CAMEL_LOG_LEVEL_FULL_DEBUG
#define CAMEL_LOG_FULL_DEBUG(args...) camel_log(CAMEL_LOG_LEVEL_FULL_DEBUG, ##args)
#else  /* CAMEL_LOG_LEVEL_FULL_DEBUG */
#define CAMEL_LOG_FULL_DEBUG(args...) 
#endif /* CAMEL_LOG_LEVEL_FULL_DEBUG */




extern void camel_log(guint level, const gchar *format, ... );

#endif /* CAMEL_LOG_H */
