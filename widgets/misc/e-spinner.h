/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright © 2000 Eazel, Inc.
 * Copyright © 2004, 2006 Christian Persch
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * $Id: ephy-spinner.h 12639 2006-12-12 17:06:55Z chpe $
 */

/*
 * From Nautilus ephy-spinner.h 
 */
#ifndef E_SPINNER_H
#define E_SPINNER_H

#include <gtk/gtkwidget.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define E_TYPE_SPINNER		(e_spinner_get_type ())
#define E_SPINNER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_SPINNER, ESpinner))
#define E_SPINNER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_SPINNER, ESpinnerClass))
#define E_IS_SPINNER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_SPINNER))
#define E_IS_SPINNER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_SPINNER))
#define E_SPINNER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), E_TYPE_SPINNER, ESpinnerClass))

typedef struct _ESpinner		ESpinner;
typedef struct _ESpinnerClass	ESpinnerClass;
typedef struct _ESpinnerDetails	ESpinnerDetails;

struct _ESpinner
{
	GtkWidget parent;

	/*< private >*/
	ESpinnerDetails *details;
};

struct _ESpinnerClass
{
	GtkWidgetClass parent_class;
};

GType		e_spinner_get_type	(void);

GtkWidget      *e_spinner_new	(void);

void		e_spinner_start	(ESpinner *throbber);

void		e_spinner_stop	(ESpinner *throbber);

void		e_spinner_set_size	(ESpinner *spinner,
					 GtkIconSize size);

G_END_DECLS

#endif /* E_SPINNER_H */
