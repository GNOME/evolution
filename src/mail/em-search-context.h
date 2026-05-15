/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Not Zed <notzed@lostzed.mmc.com.au>
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
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
