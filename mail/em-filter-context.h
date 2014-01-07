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

#ifndef EM_FILTER_CONTEXT_H
#define EM_FILTER_CONTEXT_H

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define EM_TYPE_FILTER_CONTEXT \
	(em_filter_context_get_type ())
#define EM_FILTER_CONTEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FILTER_CONTEXT, EMFilterContext))
#define EM_FILTER_CONTEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FILTER_CONTEXT, EMFilterContextClass))
#define EM_IS_FILTER_CONTEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FILTER_CONTEXT))
#define EM_IS_FILTER_CONTEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FILTER_CONTEXT))
#define EM_FILTER_CONTEXT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FILTER_CONTEXT, EMFilterContextClass))

G_BEGIN_DECLS

typedef struct _EMFilterContext EMFilterContext;
typedef struct _EMFilterContextClass EMFilterContextClass;
typedef struct _EMFilterContextPrivate EMFilterContextPrivate;

struct _EMFilterContext {
	ERuleContext parent;
	EMFilterContextPrivate *priv;
};

struct _EMFilterContextClass {
	ERuleContextClass parent_class;
};

GType		em_filter_context_get_type	(void);
EMFilterContext *
		em_filter_context_new		(EMailSession *session);
EMailSession *	em_filter_context_get_session	(EMFilterContext *context);
void		em_filter_context_add_action	(EMFilterContext *context,
						 EFilterPart *action);
EFilterPart *	em_filter_context_find_action	(EMFilterContext *context,
						 const gchar *name);
EFilterPart *	em_filter_context_create_action	(EMFilterContext *context,
						 const gchar *name);
EFilterPart *	em_filter_context_next_action	(EMFilterContext *context,
						 EFilterPart *last);

G_END_DECLS

#endif /* EM_FILTER_CONTEXT_H */
