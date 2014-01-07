/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_SEARCH_CONTEXT_H
#define EM_SEARCH_CONTEXT_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define EM_SEARCH_TYPE_CONTEXT \
	(em_search_context_get_type ())
#define EM_SEARCH_CONTEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_SEARCH_TYPE_CONTEXT, EMSearchContext))
#define EM_SEARCH_CONTEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_SEARCH_TYPE_CONTEXT, EMSearchContextClass))
#define EM_IS_SEARCH_CONTEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_SEARCH_TYPE_CONTEXT))
#define EM_IS_SEARCH_CONTEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_SEARCH_TYPE_CONTEXT))
#define EM_SEARCH_CONTEXT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_SEARCH_TYPE_CONTEXT, EMSearchContextClass))

G_BEGIN_DECLS

typedef struct _EMSearchContext EMSearchContext;
typedef struct _EMSearchContextClass EMSearchContextClass;

struct _EMSearchContext {
	ERuleContext parent;
};

struct _EMSearchContextClass {
	ERuleContextClass parent_class;
};

GType		em_search_context_get_type	(void);
ERuleContext *	em_search_context_new		(void);

G_END_DECLS

#endif /* EM_SEARCH_CONTEXT_H */
