/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Not Zed <notzed@lostzed.mmc.com.au>
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_FILTER_CODE_H
#define E_FILTER_CODE_H

#include <e-util/e-filter-input.h>

/* Standard GObject macros */
#define E_TYPE_FILTER_CODE \
	(e_filter_code_get_type ())
#define E_FILTER_CODE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_CODE, EFilterCode))
#define E_FILTER_CODE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_CODE, EFilterCodeClass))
#define E_IS_FILTER_CODE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_CODE))
#define E_IS_FILTER_CODE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_CODE))
#define E_FILTER_CODE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_CODE, EFilterCodeClass))

G_BEGIN_DECLS

typedef struct _EFilterCode EFilterCode;
typedef struct _EFilterCodeClass EFilterCodeClass;
typedef struct _EFilterCodePrivate EFilterCodePrivate;

struct _EFilterCode {
	EFilterInput parent;
	EFilterCodePrivate *priv;
};

struct _EFilterCodeClass {
	EFilterInputClass parent_class;
};

GType		e_filter_code_get_type		(void) G_GNUC_CONST;
EFilterCode *	e_filter_code_new		(gboolean raw_code);

G_END_DECLS

#endif /* E_FILTER_CODE_H */
