/*
 * Copyright (C) 2021 Red Hat (www.redhat.com)
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_FILTER_LABEL_H
#define E_FILTER_LABEL_H

#include <e-util/e-filter-element.h>

/* Standard GObject macros */
#define E_TYPE_FILTER_LABEL \
	(e_filter_label_get_type ())
#define E_FILTER_LABEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_LABEL, EFilterLabel))
#define E_FILTER_LABEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_LABEL, EFilterLabelClass))
#define E_IS_FILTER_LABEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_LABEL))
#define E_IS_FILTER_LABEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_LABEL))
#define E_FILTER_LABEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_LABEL, EFilterLabelClass))

G_BEGIN_DECLS

typedef struct _EFilterLabel EFilterLabel;
typedef struct _EFilterLabelClass EFilterLabelClass;
typedef struct _EFilterLabelPrivate EFilterLabelPrivate;

struct _EFilterLabel {
	EFilterElement parent;
	EFilterLabelPrivate *priv;
};

struct _EFilterLabelClass {
	EFilterElementClass parent_class;
};

GType		e_filter_label_get_type	(void) G_GNUC_CONST;
EFilterElement *e_filter_label_new		(void);
void		e_filter_label_set_title	(EFilterLabel *option,
						 const gchar *title);
const gchar *	e_filter_label_get_title	(EFilterLabel *option);

G_END_DECLS

#endif /* E_FILTER_LABEL_H */
