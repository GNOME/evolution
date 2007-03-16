/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __EM_STRIPSIG_FILTER_H__
#define __EM_STRIPSIG_FILTER_H__

#include <camel/camel-mime-filter.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EM_TYPE_STRIPSIG_FILTER            (em_stripsig_filter_get_type ())
#define EM_STRIPSIG_FILTER(obj)            (CAMEL_CHECK_CAST ((obj), EM_TYPE_STRIPSIG_FILTER, EMStripSigFilter))
#define EM_STRIPSIG_FILTER_CLASS(klass)    (CAMEL_CHECK_CLASS_CAST ((klass), EM_TYPE_STRIPSIG_FILTER, EMStripSigFilterClass))
#define EM_IS_STRIPSIG_FILTER(obj)         (CAMEL_CHECK_TYPE ((obj), EM_TYPE_STRIPSIG_FILTER))
#define EM_IS_STRIPSIG_FILTER_CLASS(klass) (CAMEL_CHECK_CLASS_TYPE ((klass), EM_TYPE_STRIPSIG_FILTER))
#define EM_STRIPSIG_FILTER_GET_CLASS(obj)  (CAMEL_CHECK_GET_CLASS ((obj), EM_TYPE_STRIPSIG_FILTER, EMStripSigFilterClass))

typedef struct _EMStripSigFilter EMStripSigFilter;
typedef struct _EMStripSigFilterClass EMStripSigFilterClass;

struct _EMStripSigFilter {
	CamelMimeFilter parent_object;
	
	guint32 midline:1;
};

struct _EMStripSigFilterClass {
	CamelMimeFilterClass parent_class;
	
};


CamelType em_stripsig_filter_get_type (void);

CamelMimeFilter *em_stripsig_filter_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_STRIPSIG_FILTER_H__ */
