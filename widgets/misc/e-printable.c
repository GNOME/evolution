/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-printable.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtk.h>

#include "e-util/e-util-marshal.h"

#include "e-util/e-util.h"

#include "e-printable.h"

#define EP_CLASS(e) ((EPrintableClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE GTK_TYPE_OBJECT
					  

static GtkObjectClass *e_printable_parent_class;

enum {
	PRINT_PAGE,
	DATA_LEFT,
	RESET,
	HEIGHT,
	WILL_FIT,
	LAST_SIGNAL
};

static guint e_printable_signals [LAST_SIGNAL] = { 0, };

static void
e_printable_class_init (GtkObjectClass *object_class)
{
	EPrintableClass *klass = E_PRINTABLE_CLASS(object_class);
	e_printable_parent_class = g_type_class_ref (PARENT_TYPE);
	
	e_printable_signals [PRINT_PAGE] =
		g_signal_new ("print_page",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EPrintableClass, print_page),
			      NULL, NULL,
			      e_util_marshal_NONE__OBJECT_DOUBLE_DOUBLE_BOOLEAN,
			      G_TYPE_NONE, 4, G_TYPE_OBJECT, G_TYPE_DOUBLE,
			      G_TYPE_DOUBLE, G_TYPE_BOOLEAN);

	e_printable_signals [DATA_LEFT] =
		g_signal_new ("data_left",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EPrintableClass, data_left),
			      NULL, NULL,
			      e_util_marshal_BOOLEAN__NONE,
			      G_TYPE_BOOLEAN, 0, G_TYPE_NONE);

	e_printable_signals [RESET] =
		g_signal_new ("reset",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EPrintableClass, reset),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0, G_TYPE_NONE);

	e_printable_signals [HEIGHT] =
		g_signal_new ("height",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EPrintableClass, height),
			      NULL, NULL,
			      e_util_marshal_DOUBLE__OBJECT_DOUBLE_DOUBLE_BOOLEAN,
			      G_TYPE_DOUBLE, 4, G_TYPE_OBJECT, G_TYPE_DOUBLE,
			      G_TYPE_DOUBLE, G_TYPE_BOOLEAN);
	
	e_printable_signals [WILL_FIT] =
		g_signal_new ("will_fit",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EPrintableClass, will_fit),
			      NULL, NULL,
			      e_util_marshal_BOOLEAN__OBJECT_DOUBLE_DOUBLE_BOOLEAN,
			      G_TYPE_BOOLEAN, 4, G_TYPE_OBJECT, G_TYPE_DOUBLE,
			      G_TYPE_DOUBLE, G_TYPE_BOOLEAN);

	klass->print_page = NULL;    
	klass->data_left = NULL;
	klass->reset = NULL;
	klass->height = NULL;
	klass->will_fit = NULL;
}


E_MAKE_TYPE (e_printable,
	     "EPrintable",
	     EPrintable,
	     e_printable_class_init,
	     NULL,
	     PARENT_TYPE)

EPrintable *
e_printable_new(void)
{
	return E_PRINTABLE(g_object_new(E_PRINTABLE_TYPE, NULL));
}

void
e_printable_print_page          (EPrintable        *e_printable,
				 GnomePrintContext *context,
				 gdouble            width,
				 gdouble            height,
				 gboolean           quantized)
{
	g_return_if_fail (e_printable != NULL);
	g_return_if_fail (E_IS_PRINTABLE (e_printable));
	
	g_signal_emit (e_printable,
		       e_printable_signals [PRINT_PAGE], 0,
		       context,
		       width,
		       height,
		       quantized);
}

gboolean
e_printable_data_left           (EPrintable        *e_printable)
{
        gboolean ret_val;

	g_return_val_if_fail (e_printable != NULL, FALSE);
	g_return_val_if_fail (E_IS_PRINTABLE (e_printable), FALSE);

	g_signal_emit (e_printable,
		       e_printable_signals [DATA_LEFT], 0,
		       &ret_val);

	return ret_val;
}

void
e_printable_reset               (EPrintable        *e_printable)
{
	g_return_if_fail (e_printable != NULL);
	g_return_if_fail (E_IS_PRINTABLE (e_printable));
	
	g_signal_emit (e_printable,
		       e_printable_signals [RESET], 0);
}

gdouble
e_printable_height              (EPrintable        *e_printable,
				 GnomePrintContext *context,
				 gdouble            width,
				 gdouble            max_height,
				 gboolean           quantized)
{
        gdouble ret_val;

	g_return_val_if_fail (e_printable != NULL, -1);
	g_return_val_if_fail (E_IS_PRINTABLE (e_printable), -1);

	g_signal_emit (e_printable,
		       e_printable_signals [HEIGHT], 0,
		       context,
		       width,
		       max_height,
		       quantized,
		       &ret_val);

	return ret_val;
}

gboolean
e_printable_will_fit            (EPrintable        *e_printable,
				 GnomePrintContext *context,
				 gdouble            width,
				 gdouble            max_height,
				 gboolean           quantized)
{
        gboolean ret_val;

	g_return_val_if_fail (e_printable != NULL, -1);
	g_return_val_if_fail (E_IS_PRINTABLE (e_printable), -1);

	g_signal_emit (e_printable,
		       e_printable_signals [WILL_FIT], 0,
		       context,
		       width,
		       max_height,
		       quantized,
		       &ret_val);

	return ret_val;
}
