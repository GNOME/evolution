/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *  JP Rosevear <jpr@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef _E_SELECT_NAMES_CONFIG_KEYS_H_
#define _E_SELECT_NAMES_CONFIG_KEYS_H_

#include <glib.h>

G_BEGIN_DECLS

#define SELECT_NAMES_CONFIG_PREFIX "/apps/evolution/addressbook/completion"

/* Display settings */
#define SELECT_NAMES_CONFIG_COMPLETION_BOOKS SELECT_NAMES_CONFIG_PREFIX "/books"
#define SELECT_NAMES_CONFIG_LAST_COMPLETION_BOOK SELECT_NAMES_CONFIG_PREFIX "/last_book"
#define SELECT_NAMES_CONFIG_MIN_QUERY_LENGTH SELECT_NAMES_CONFIG_PREFIX "/minimum_query_length"

G_END_DECLS

#endif
