/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-pane.c: A paned window which accepts more than one child
 *
 * Author:
 *   Matt Loper (matt@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc
 */

#include "e-paned.h"
#include "e-util.h"

#define PARENT_TYPE gtk_frame_get_type ()
static GtkObjectClass *e_paned_parent_class;

/*----------------------------------------------------------------------*
 *                         (un)parenting functions
 *----------------------------------------------------------------------*/

static void
unparent_all_children (EPaned *paned)
{
	GList *l;
	
	for (l = paned->children; l != NULL; l = l->next) {
		GtkWidget *child = GTK_WIDGET (l->data);
		
		gtk_widget_ref (child);
		gtk_container_remove (GTK_CONTAINER (child->parent),
				      child);
	}

	if (paned->toplevel_paned) {
		GtkWidget* parent =
			GTK_WIDGET (paned->toplevel_paned)->parent;
		
		gtk_container_remove (GTK_CONTAINER (parent),
				      GTK_WIDGET (paned->toplevel_paned));
	}
		

	paned->toplevel_paned = NULL;
}

static GtkPaned*
new_gtk_paned (EPaned *paned)
{
	return paned->horizontal?
		GTK_PANED (gtk_hpaned_new ()):
		GTK_PANED (gtk_vpaned_new ());
}


static void
reparent_all_children (EPaned *paned)
{
	GtkPaned *cur_gtk_paned;
	GList *l = paned->children;
	int requested_size;

	g_assert (E_IS_PANED (paned));
	
	if (paned->toplevel_paned)
		unparent_all_children (paned);

	if (!l)
		return;
	
	/* if there's only one child in our list, we don't need a
	   splitter window; we can just show the one window */
	if (!l->next)
	{
		gtk_container_add (GTK_CONTAINER (paned),
				   GTK_WIDGET (l->data));
		return;
	}

	/* create a gtk_paned, and put it in our toplevel EPaned */
	cur_gtk_paned = new_gtk_paned (paned);
	paned->toplevel_paned = cur_gtk_paned;
	gtk_container_add (GTK_CONTAINER (paned),
			   GTK_WIDGET (paned->toplevel_paned));

	/* put the first widget in the left part of our pane,
	   and give it the amount of space requested */
	gtk_paned_add1 (cur_gtk_paned, GTK_WIDGET (l->data));
	requested_size = (int)gtk_object_get_data (
		GTK_OBJECT (l->data),
		"e_paned_requested_size");
	gtk_paned_set_position (GTK_PANED (cur_gtk_paned),
				requested_size);
	
	l = l->next;

	for (; l != NULL; l = l->next) {
		
		if (l->next) {

			GtkPaned *sub_gtk_paned =
				new_gtk_paned (paned);
			GtkWidget *w = GTK_WIDGET (l->data);

                        /* add our widget to the new subpane,
			   on the left */
			gtk_paned_add1 (sub_gtk_paned, w);

			requested_size = (int)gtk_object_get_data (
				GTK_OBJECT (w),
				"e_paned_requested_size");
			gtk_paned_set_position (GTK_PANED (sub_gtk_paned),
						requested_size);
	
			gtk_paned_add2 (cur_gtk_paned,
					GTK_WIDGET (sub_gtk_paned));
			cur_gtk_paned = sub_gtk_paned;
		}
		else {
			gtk_paned_add2 (cur_gtk_paned,
					GTK_WIDGET (l->data));
		}		
	}
}

/*----------------------------------------------------------------------*
 *                  Exposed regular functions
 *----------------------------------------------------------------------*/


/**
 * e_paned_insert:
 * @paned: the #EPaned object
 * @pos: the position where we should insert the widget
 * @child: the widget to insert in the #EPaned object
 * @requested_size: the requested span of the widget, which will be
 * width of the #EPaned is horizontal, or height if it's vertical
 *
 * Inserts a widget into the #EPaned window, given a requested size
 * and a position; the position specifies where, among the other
 * widgets, the widget should be placed.
 *
 **/
void
e_paned_insert (EPaned *paned, int pos, GtkWidget *child, int requested_size)
{
	g_assert (GTK_IS_WIDGET (child));
	g_assert (E_IS_PANED (paned));
	
	unparent_all_children (paned);
	
	paned->children = g_list_insert (paned->children, child, pos);
	gtk_object_set_data (GTK_OBJECT (child),
			     "e_paned_requested_size",
			     (gpointer)requested_size);
	
	reparent_all_children (paned);

	g_print ("%s: %s(): exiting, length is %i\n",
		 __FILE__, __FUNCTION__, g_list_length (paned->children));	
}

/**
 * e_paned_remove:
 * @paned: the #EPaned object
 * @removed_child: the widget to remove
 *
 * Removes a widget from an #EPaned widget.
 *
 **/
void
e_paned_remove      (EPaned *paned, GtkWidget *removed_child)
{
	unparent_all_children (paned);
	paned->children = g_list_remove (paned->children, removed_child);
	gtk_widget_unref (GTK_WIDGET (removed_child));
	reparent_all_children (paned);	
}


/*----------------------------------------------------------------------*
 *                     Standard Gtk+ Class functions
 *----------------------------------------------------------------------*/

void
e_paned_construct (EPaned *e_paned,
		   gboolean horizontal)
{
	g_return_if_fail (e_paned != NULL);
	g_return_if_fail (E_IS_PANED (e_paned));

	
	e_paned->horizontal = horizontal;
}

GtkWidget*
e_paned_new (gboolean horizontal)
{
	EPaned *e_paned;

	e_paned = gtk_type_new (e_paned_get_type ());

	e_paned_construct (e_paned, horizontal);

	g_assert (E_IS_PANED (e_paned));

	return GTK_WIDGET (e_paned);
}

static void
e_paned_init (GtkObject *object)
{
	EPaned *e_paned;

	e_paned = E_PANED (object);

	e_paned->children = NULL;
	e_paned->toplevel_paned = NULL;
	e_paned->horizontal = FALSE;
}

static void
e_paned_class_init (GtkObjectClass *object_class)
{
	e_paned_parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE(e_paned, "EPaned", EPaned, e_paned_class_init, e_paned_init, PARENT_TYPE);
