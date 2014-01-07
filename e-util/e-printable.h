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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_PRINTABLE_H
#define E_PRINTABLE_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_PRINTABLE \
	(e_printable_get_type ())
#define E_PRINTABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PRINTABLE, EPrintable))
#define E_PRINTABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PRINTABLE, EPrintableClass))
#define E_IS_PRINTABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PRINTABLE))
#define E_IS_PRINTABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PRINTABLE))
#define E_PRINTABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PRINTABLE, EPrintableClass))

G_BEGIN_DECLS

typedef struct _EPrintable EPrintable;
typedef struct _EPrintableClass EPrintableClass;

struct _EPrintable {
	GObject parent;
};

struct _EPrintableClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*print_page)		(EPrintable *printable,
						 GtkPrintContext *context,
						 gdouble width,
						 gdouble height,
						 gboolean quantized);
	gboolean	(*data_left)		(EPrintable *printable);
	void		(*reset)		(EPrintable *printable);
	gdouble		(*height)		(EPrintable *printable,
						 GtkPrintContext *context,
						 gdouble width,
						 gdouble max_height,
						 gboolean quantized);

	/* e_printable_will_fit (ep, ...) should be equal in value to
	 * (e_printable_print_page (ep, ...),
	 * !e_printable_data_left(ep)) except that the latter has the
	 * side effect of doing the printing and advancing the
	 * position of the printable.
	 */
	gboolean	(*will_fit)		(EPrintable *printable,
						 GtkPrintContext *context,
						 gdouble width,
						 gdouble max_height,
						 gboolean quantized);
};

GType		e_printable_get_type		(void) G_GNUC_CONST;
EPrintable *	e_printable_new			(void);
void		e_printable_print_page		(EPrintable *e_printable,
						 GtkPrintContext *context,
						 gdouble width,
						 gdouble height,
						 gboolean quantized);
gboolean	e_printable_data_left		(EPrintable *printable);
void		e_printable_reset		(EPrintable *printable);
gdouble		e_printable_height		(EPrintable *printable,
						 GtkPrintContext *context,
						 gdouble width,
						 gdouble max_height,
						 gboolean quantized);
gboolean	e_printable_will_fit		(EPrintable *printable,
						 GtkPrintContext *context,
						 gdouble width,
						 gdouble max_height,
						 gboolean quantized);

G_END_DECLS

#endif /* E_PRINTABLE_H */
