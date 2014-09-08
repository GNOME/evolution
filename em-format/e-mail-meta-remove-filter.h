/*
 * e-mail-meta-remove-filter.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_MAIL_META_REMOVE_FILTER_H
#define E_MAIL_META_REMOVE_FILTER_H

#include <camel/camel.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_META_REMOVE_FILTER \
	(e_mail_meta_remove_filter_get_type ())
#define E_MAIL_META_REMOVE_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_META_REMOVE_FILTER, EMailmeta_removeFilter))
#define E_MAIL_META_REMOVE_FILTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_META_REMOVE_FILTER, EMailmeta_removeFilterClass))
#define E_IS_MAIL_META_REMOVE_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_META_REMOVE_FILTER))
#define E_IS_MAIL_META_REMOVE_FILTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_META_REMOVE_FILTER))
#define E_MAIL_META_REMOVE_FILTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_META_REMOVE_FILTER, EMailmeta_removeFilterClass))

G_BEGIN_DECLS

typedef struct _EMailMetaRemoveFilter EMailMetaRemoveFilter;
typedef struct _EMailMetaRemoveFilterClass EMailMetaRemoveFilterClass;

struct _EMailMetaRemoveFilter {
	CamelMimeFilter parent;

	gboolean remove_all_meta;
	gboolean in_meta;
	gboolean after_head;
};

struct _EMailMetaRemoveFilterClass {
	CamelMimeFilterClass parent_class;
};

GType		e_mail_meta_remove_filter_get_type	(void);
CamelMimeFilter *
		e_mail_meta_remove_filter_new		(gboolean remove_all_meta);

G_END_DECLS

#endif /* E_MAIL_META_REMOVE_FILTER_H */
