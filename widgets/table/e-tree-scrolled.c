/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree-scrolled.c
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtksignal.h>
#include <parser.h>
#include <xmlmemory.h>
#include <gal/util/e-util.h>

#include "e-tree-scrolled.h"

#define COLUMN_HEADER_HEIGHT 16

#define PARENT_TYPE e_scroll_frame_get_type ()

static GtkObjectClass *parent_class;

enum {
	ARG_0,
	ARG_TREE,
};

static void
e_tree_scrolled_init (GtkObject *object)
{
	ETreeScrolled *ets;
	EScrollFrame *scroll_frame;

	ets          = E_TREE_SCROLLED (object);
	scroll_frame = E_SCROLL_FRAME   (object);

	GTK_WIDGET_SET_FLAGS (ets, GTK_CAN_FOCUS);

	ets->tree = gtk_type_new(e_tree_get_type());

	e_scroll_frame_set_policy      (scroll_frame, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	e_scroll_frame_set_shadow_type (scroll_frame, GTK_SHADOW_IN);
}

static void
e_tree_scrolled_real_construct (ETreeScrolled *ets)
{
	gtk_container_add(GTK_CONTAINER(ets), GTK_WIDGET(ets->tree));

	gtk_widget_show(GTK_WIDGET(ets->tree));
}

ETreeScrolled *e_tree_scrolled_construct                 (ETreeScrolled    *ets,
							    ETreeModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec,
							    const char        *state)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_SCROLLED(ets), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec != NULL, NULL);

	e_tree_construct(ets->tree, etm, ete, spec, state);

	e_tree_scrolled_real_construct(ets);

	return ets;
}

GtkWidget      *e_tree_scrolled_new                       (ETreeModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec,
							    const char        *state)
{
	ETreeScrolled *ets;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec != NULL, NULL);

	ets = E_TREE_SCROLLED (gtk_widget_new (e_tree_scrolled_get_type (),
						"hadjustment", NULL,
						"vadjustment", NULL,
						NULL));

	ets = e_tree_scrolled_construct (ets, etm, ete, spec, state);

	return GTK_WIDGET (ets);
}

ETreeScrolled *e_tree_scrolled_construct_from_spec_file  (ETreeScrolled    *ets,
							    ETreeModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec_fn,
							    const char        *state_fn)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_SCROLLED(ets), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_fn != NULL, NULL);

	e_tree_construct_from_spec_file(ets->tree, etm, ete, spec_fn, state_fn);

	e_tree_scrolled_real_construct(ets);

	return ets;
}

GtkWidget      *e_tree_scrolled_new_from_spec_file        (ETreeModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec_fn,
							    const char        *state_fn)
{
	ETreeScrolled *ets;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_fn != NULL, NULL);

	ets = E_TREE_SCROLLED (gtk_widget_new (e_tree_scrolled_get_type (),
                                                "hadjustment", NULL,
                                                "vadjustment", NULL,
                                                NULL));
	ets = e_tree_scrolled_construct_from_spec_file (ets, etm, ete, spec_fn, state_fn);

	return GTK_WIDGET (ets);
}

ETree *
e_tree_scrolled_get_tree                 (ETreeScrolled *ets)
{
	return ets->tree;
}

static void
ets_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETreeScrolled *ets = E_TREE_SCROLLED (o);

	switch (arg_id){
	case ARG_TREE:
		if (ets->tree)
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(ets->tree);
		else
			GTK_VALUE_OBJECT (*arg) = NULL;
		break;
	}
}

/* Grab_focus handler for the scrolled ETree */
static void
ets_grab_focus (GtkWidget *widget)
{
	ETreeScrolled *ets;

	ets = E_TREE_SCROLLED (widget);

	gtk_widget_grab_focus (GTK_WIDGET (ets->tree));
}

/* Focus handler for the scrolled ETree */
static gint
ets_focus (GtkContainer *container, GtkDirectionType direction)
{
	ETreeScrolled *ets;

	ets = E_TREE_SCROLLED (container);

	return gtk_container_focus (GTK_CONTAINER (ets->tree), direction);
}

static void
e_tree_scrolled_class_init (ETreeScrolledClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	container_class = (GtkContainerClass *) class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->get_arg = ets_get_arg;

	widget_class->grab_focus = ets_grab_focus;

	container_class->focus = ets_focus;

	gtk_object_add_arg_type ("ETreeScrolled::tree", GTK_TYPE_OBJECT,
				 GTK_ARG_READABLE, ARG_TREE);
}

E_MAKE_TYPE(e_tree_scrolled, "ETreeScrolled", ETreeScrolled, e_tree_scrolled_class_init, e_tree_scrolled_init, PARENT_TYPE);

