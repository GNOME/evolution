/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HEADER_BAR_H
#define E_HEADER_BAR_H

#include <gtk/gtk.h>

#define E_TYPE_HEADER_BAR \
	(e_header_bar_get_type ())
#define E_HEADER_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HEADER_BAR, EHeaderBar))
#define E_HEADER_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HEADER_BAR, EHeaderBarClass))
#define E_IS_HEADER_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HEADER_BAR))
#define E_IS_HEADER_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HEADER_BAR))
#define E_HEADER_BAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HEADER_BAR, EHeaderBarClass))

G_BEGIN_DECLS

typedef struct _EHeaderBar EHeaderBar;
typedef struct _EHeaderBarClass EHeaderBarClass;
typedef struct _EHeaderBarPrivate EHeaderBarPrivate;

struct _EHeaderBar {
	GtkHeaderBar parent;
	EHeaderBarPrivate *priv;
};

struct _EHeaderBarClass {
	GtkHeaderBarClass parent_class;
};

GType			e_header_bar_get_type		(void);
GtkWidget *		e_header_bar_new		(void);
void			e_header_bar_pack_start		(EHeaderBar *self,
							 GtkWidget *widget,
							 guint label_priority);
void			e_header_bar_pack_end		(EHeaderBar *self,
							 GtkWidget *widget,
							 guint label_priority);
void			e_header_bar_remove_all		(EHeaderBar *self);
GList *			e_header_bar_get_start_widgets	(EHeaderBar *self);
GList *			e_header_bar_get_end_widgets	(EHeaderBar *self);

G_END_DECLS

#endif /* E_HEADER_BAR_H */
