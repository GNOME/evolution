/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _FILTER_CODE_H
#define _FILTER_CODE_H

#include <gtk/gtk.h>

#include "filter-input.h"

#define FILTER_CODE(obj)	GTK_CHECK_CAST (obj, filter_code_get_type (), FilterCode)
#define FILTER_CODE_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, filter_code_get_type (), FilterCodeClass)
#define IS_FILTER_CODE(obj)      GTK_CHECK_TYPE (obj, filter_code_get_type ())

typedef struct _FilterCode	FilterCode;
typedef struct _FilterCodeClass	FilterCodeClass;

struct _FilterCode {
	FilterInput parent;
};

struct _FilterCodeClass {
	FilterInputClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		filter_code_get_type	(void);
FilterCode	*filter_code_new	(void);

/* methods */

#endif /* ! _FILTER_CODE_H */

