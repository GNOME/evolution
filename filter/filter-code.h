/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
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
 */


#ifndef _FILTER_CODE_H
#define _FILTER_CODE_H

#include "filter-input.h"

#define FILTER_TYPE_CODE            (filter_code_get_type ())
#define FILTER_CODE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_CODE, FilterCode))
#define FILTER_CODE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_CODE, FilterCodeClass))
#define IS_FILTER_CODE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_CODE))
#define IS_FILTER_CODE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_CODE))
#define FILTER_CODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_CODE, FilterCodeClass))

typedef struct _FilterCode FilterCode;
typedef struct _FilterCodeClass	FilterCodeClass;

struct _FilterCode {
	FilterInput parent_object;
};

struct _FilterCodeClass {
	FilterInputClass parent_class;

	/* virtual methods */

	/* signals */
};

GType filter_code_get_type (void);
FilterCode *filter_code_new (void);

/* methods */

#endif /* ! _FILTER_CODE_H */
