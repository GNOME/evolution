/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SPLIT_DATE_EDIT_H
#define E_SPLIT_DATE_EDIT_H

#include <gtk/gtk.h>

#define E_TYPE_SPLIT_DATE_EDIT (e_split_date_edit_get_type ())

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (ESplitDateEdit, e_split_date_edit, E, SPLIT_DATE_EDIT, GtkGrid)

GtkWidget *	e_split_date_edit_new		(void);
void		e_split_date_edit_set_format	(ESplitDateEdit *self,
						 const gchar *format);
const gchar *	e_split_date_edit_get_format	(ESplitDateEdit *self);
void		e_split_date_edit_set_ymd	(ESplitDateEdit *self,
						 guint year,
						 guint month,
						 guint day);
void		e_split_date_edit_get_ymd	(ESplitDateEdit *self,
						 guint *out_year,
						 guint *out_month,
						 guint *out_day);

G_END_DECLS

#endif /* E_SPLIT_DATE_EDIT_H */
