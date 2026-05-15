/*
 * SPDX-FileCopyrightText: (C) 2014 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Milan Crha <mcrha@redhat.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SPINNER_H
#define E_SPINNER_H

#include <gtk/gtk.h>

#define E_TYPE_SPINNER		(e_spinner_get_type ())
#define E_SPINNER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_SPINNER, ESpinner))
#define E_SPINNER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_SPINNER, ESpinnerClass))
#define E_IS_SPINNER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_SPINNER))
#define E_IS_SPINNER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_SPINNER))
#define E_SPINNER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), E_TYPE_SPINNER, ESpinnerClass))

G_BEGIN_DECLS

typedef struct _ESpinner	ESpinner;
typedef struct _ESpinnerClass	ESpinnerClass;
typedef struct _ESpinnerPrivate	ESpinnerPrivate;

struct _ESpinner
{
	GtkImage parent;

	/*< private >*/
	ESpinnerPrivate *priv;
};

struct _ESpinnerClass
{
	GtkImageClass parent_class;
};

GType		e_spinner_get_type	(void);

GtkWidget *	e_spinner_new		(void);
gboolean	e_spinner_get_active	(ESpinner *spinner);
void		e_spinner_set_active	(ESpinner *spinner,
					 gboolean active);
void		e_spinner_start		(ESpinner *spinner);
void		e_spinner_stop		(ESpinner *spinner);

G_END_DECLS

#endif /* E_SPINNER_H */
