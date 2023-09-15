/*
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_FILTER_PART_H
#define E_FILTER_PART_H

#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <e-util/e-alert.h>
#include <e-util/e-filter-element.h>

/* Standard GObject macros */
#define E_TYPE_FILTER_PART \
	(e_filter_part_get_type ())
#define E_FILTER_PART(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_PART, EFilterPart))
#define E_FILTER_PART_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_PART, EFilterPartClass))
#define E_IS_FILTER_PART(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_PART))
#define E_IS_FILTER_PART_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_PART))
#define E_FILTER_PART_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_PART, EFilterPartClass))

G_BEGIN_DECLS

struct _ERuleContext;

typedef struct _EFilterPart EFilterPart;
typedef struct _EFilterPartClass EFilterPartClass;
typedef struct _EFilterPartPrivate EFilterPartPrivate;

struct _EFilterPart {
	GObject parent;
	EFilterPartPrivate *priv;

	gchar *name;
	gchar *title;
	gchar *code;
	GList *elements;
	gchar *code_gen_func;	/* function to generate the code;
				 * either @code or @code_gen_func is non-NULL,
				 * never both */
};

struct _EFilterPartClass {
	GObjectClass parent_class;
};

GType		e_filter_part_get_type		(void) G_GNUC_CONST;
EFilterPart *	e_filter_part_new		(void);
gboolean	e_filter_part_validate		(EFilterPart *part,
						 EAlert **alert);
gint		e_filter_part_eq		(EFilterPart *part_a,
						 EFilterPart *part_b);
gint		e_filter_part_xml_create	(EFilterPart *part,
						 xmlNodePtr node,
						 struct _ERuleContext *rc);
xmlNodePtr	e_filter_part_xml_encode	(EFilterPart *fe);
gint		e_filter_part_xml_decode	(EFilterPart *fe,
						 xmlNodePtr node);
EFilterPart *	e_filter_part_clone		(EFilterPart *part);
void		e_filter_part_copy_values	(EFilterPart *dst_part,
						 EFilterPart *src_part);
EFilterElement *e_filter_part_find_element	(EFilterPart *part,
						 const gchar *name);
GtkWidget *	e_filter_part_get_widget	(EFilterPart *part);
void		e_filter_part_describe		(EFilterPart *part,
						 GString *out);
void		e_filter_part_build_code	(EFilterPart *part,
						 GString *out);
void		e_filter_part_expand_code	(EFilterPart *part,
						 const gchar *str,
						 GString *out);

void		e_filter_part_build_code_list	(GList *list,
						 GString *out);
EFilterPart *	e_filter_part_find_list		(GList *list,
						 const gchar *name);
EFilterPart *	e_filter_part_next_list		(GList *list,
						 EFilterPart *last);

G_END_DECLS

#endif /* E_FILTER_PART_H */
