/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-Table-Group.c: Implements the grouping objects for elements on a table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org()
 *
 * Copyright 1999, 2000 Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-group-container.h"
#include "e-table-item.h"
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "e-util/e-util.h"
#include "e-util/e-canvas.h"
#include "e-util/e-canvas-utils.h"
#include "widgets/e-text/e-text.h"

#define TITLE_HEIGHT         16
#define GROUP_INDENT         10

#define BUTTON_HEIGHT        10
#define BUTTON_PADDING       2

#define PARENT_TYPE e_table_group_get_type ()

static GnomeCanvasGroupClass *etgc_parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_HEIGHT,
	ARG_WIDTH,
	ARG_FROZEN
};

typedef struct {
	ETableGroup *child;
	void *key;
	GnomeCanvasItem *text;
	GnomeCanvasItem *rect;
	gint count;
} ETableGroupContainerChildNode;

static void
e_table_group_container_child_node_free(ETableGroupContainer          *etgc,
					ETableGroupContainerChildNode *child_node)
{
	ETableGroup *etg = E_TABLE_GROUP (etgc);
	ETableGroup *child = child_node->child;

	gtk_object_destroy (GTK_OBJECT (child));
	e_table_model_free_value (etg->model, etgc->ecol->col_idx,
				  child_node->key);
	gtk_object_destroy (GTK_OBJECT (child_node->text));
	gtk_object_destroy (GTK_OBJECT (child_node->rect));
}

static void
e_table_group_container_list_free(ETableGroupContainer *etgc)
{
	ETableGroupContainerChildNode *child_node;
	GList *list;

	if (etgc->idle)
		g_source_remove (etgc->idle);

	for (list = etgc->children; list; list = g_list_next (list)) {
		child_node = (ETableGroupContainerChildNode *) list->data;
		e_table_group_container_child_node_free (etgc, child_node);
	}

	g_list_free (etgc->children);
}

static void
etgc_destroy (GtkObject *object)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (object);

	if ( etgc->font ) {
		gdk_font_unref(etgc->font);
		etgc->font = NULL;
	}
	if ( etgc->ecol ) {
		gtk_object_unref(GTK_OBJECT(etgc->ecol));
	}
	if ( etgc->rect ) {
		gtk_object_destroy(GTK_OBJECT(etgc->rect));
	}	
	e_table_group_container_list_free(etgc);

	GTK_OBJECT_CLASS (etgc_parent_class)->destroy (object);
}

#if 0
void
e_table_group_add (ETableGroup *etg, GnomeCanvasItem *item)
{
	double x1, y1, x2, y2;
	
	g_return_if_fail (etg != NULL);
	g_return_if_fail (item != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	etg->children = g_list_append (etg->children, item);

	GNOME_CANVAS_ITEM_CLASS (GTK_OBJECT (etg)->klass)->bounds (etg, &x1, &y1, &x2, &y2);
		
	if (GTK_OBJECT (etg)->flags & GNOME_CANVAS_ITEM_REALIZED){
		GList *l;
		int height = etg->transparent ? 0 : TITLE_HEIGHT;
		int x = etg->transparent ? 0 : GROUP_INDENT;
		
		for (l = etg->children; l->next; l = l->next){
			GnomeCanvasItem *child = l->data;

			height += child->y2 - child->y1;

			printf ("Height\n");
			if (E_IS_TABLE_ITEM (item)){
				printf ("    Item:  ");
			} else {
				printf ("    Group: ");
			}
			printf ("%d\n", child->y2-child->y1);
		}

		e_canvas_item_move_absolute ( item, x, height);

		
		if (E_IS_TABLE_ITEM (item)){
			
			printf ("Table item! ---------\n");
			gtk_signal_connect (GTK_OBJECT (item), "resize",
					    GTK_SIGNAL_FUNC (etg_relayout), etg);
		}
	}
}

static void
etg_realize (GnomeCanvasItem *item)
{
	ETableGroup *etg = E_TABLE_GROUP (item);
	GList *l;
	int height = 0;
	
	GNOME_CANVAS_ITEM_CLASS (etg_parent_class)->realize (item);

	for (l = etg->children; l; l = l->next){
		GnomeCanvasItem *child = l->data;

		printf ("During realization for child %p -> %d\n", child, height);
		gnome_canvas_item_set (
			child,
			"y", (double) height,
			NULL);

		height += child->y2 - child->y1;
	}
}

static void
etg_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	ETableGroup *etg = E_TABLE_GROUP (item);

	GNOME_CANVAS_ITEM_CLASS (etg_parent_class)->update (item, affine, clip_path, flags);

	if (!etg->transparent){
		int current_width, current_height;

		etg_dim (etg, &current_width, &current_height);
		
		if ((current_height != etg->height) || (current_width != etg->width)){
			etg->width = current_width;
			etg->height = current_height;
			
			gnome_canvas_item_set (
				etg->rect,
				"x1", 0.0,
				"y1", 0.0,
				"x2", (double) etg->width,
				"y2", (double) etg->height,
				NULL);
		}
	}
}
#endif

void
e_table_group_container_construct (GnomeCanvasGroup *parent, ETableGroupContainer *etgc,
				   ETableHeader *full_header,
				   ETableHeader     *header,
				   ETableModel *model, ETableCol *ecol, int ascending, xmlNode *child_rules)
{
	e_table_group_construct (parent, E_TABLE_GROUP (etgc), full_header, header, model);
	etgc->ecol = ecol;
	gtk_object_ref(GTK_OBJECT(etgc->ecol));
	etgc->child_rules = child_rules;
	etgc->ascending = ascending;
	
	etgc->font = gdk_font_load ("lucidasans-10");
	if (!etgc->font){
		etgc->font = GTK_WIDGET (GNOME_CANVAS_ITEM (etgc)->canvas)->style->font;
		
		gdk_font_ref (etgc->font);
	}
	etgc->open = TRUE;
#if 0
	etgc->transparent = transparent;
	
	etgc_dim (etgc, &etgc->width, &etgc->height);

	if (!etgc->transparent)
		etgc->rect = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (etgc),
			gnome_canvas_rect_get_type (),
			"fill_color", "gray",
			"outline_color", "gray20",
			"x1", 0.0,
			"y1", 0.0,
			"x2", (double) etgc->width,
			"y2", (double) etgc->height,
			NULL);
#endif

#if 0
	/*
	 * Reparent the child into our space.
	 */
	gnome_canvas_item_reparent (child, GNOME_CANVAS_GROUP (etgc));

	gnome_canvas_item_set (
		child,
		"x", (double) GROUP_INDENT,
		"y", (double) TITLE_HEIGHT,
		NULL);

	/*
	 * Force dimension computation
	 */
	GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->update (
		GNOME_CANVAS_ITEM (etgc), NULL, NULL, GNOME_CANVAS_UPDATE_REQUESTED);
#endif
}

ETableGroup *
e_table_group_container_new (GnomeCanvasGroup *parent, ETableHeader *full_header,
			     ETableHeader     *header,
			     ETableModel *model, ETableCol *ecol,
			     int ascending, xmlNode *child_rules)
{
	ETableGroupContainer *etgc;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (ecol != NULL, NULL);
	
	etgc = gtk_type_new (e_table_group_container_get_type ());

	e_table_group_container_construct (parent, etgc, full_header, header,
					   model, ecol, ascending, child_rules);
	return E_TABLE_GROUP (etgc);
}

#if 0
static void
etgc_relayout (GnomeCanvasItem *eti, ETableGroupContainer *etgc)
{
	GList *l;
	int height = etgc->transparent ? 0 : GROUP_INDENT;
	gboolean move = FALSE;
	
	printf ("Relaying out\n");
	
	for (l = etgc->children; l->next; l = l->next){
		GnomeCanvasItem *child = l->data;

		height += child->y2 - child->y1;

		if (child == eti)
			move = TRUE;

		if (move){
			printf ("Moving item %p\n", child);
			gnome_canvas_item_set ( child,
						"y", (double) height,
						NULL);
		}
	}
	if (height != etgc->height){
		etgc->height = height;
		gtk_signal_emit (GTK_OBJECT (etgc), etgc_signals [RESIZE]);
	}
}

void
e_table_group_container_add (ETableGroupContainer *etgc, GnomeCanvasItem *item)
{
	double x1, y1, x2, y2;
	
	g_return_if_fail (etgc != NULL);
	g_return_if_fail (item != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etgc));
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	etgc->children = g_list_append (etgc->children, item);

	GNOME_CANVAS_ITEM_CLASS (GTK_OBJECT (etgc)->klass)->bounds (etgc, &x1, &y1, &x2, &y2);
		
	if (GTK_OBJECT (etgc)->flags & GNOME_CANVAS_ITEM_REALIZED){
		GList *l;
		int height = etgc->transparent ? 0 : TITLE_HEIGHT;
		int x = etgc->transparent ? 0 : GROUP_INDENT;
		
		for (l = etgc->children; l->next; l = l->next){
			GnomeCanvasItem *child = l->data;

			height += child->y2 - child->y1;

			printf ("Height\n");
			if (E_IS_TABLE_ITEM (item)){
				printf ("    Item:  ");
			} else {
				printf ("    Group: ");
			}
			printf ("%d\n", child->y2-child->y1);
		}

		e_canvas_item_move_absolute ( item, x, height);

		
		if (E_IS_TABLE_ITEM (item)){
			
			printf ("Table item! ---------\n");
			gtk_signal_connect (GTK_OBJECT (item), "resize",
					    GTK_SIGNAL_FUNC (etgc_relayout), etgc);
		}
	}
}

static void
etgc_realize (GnomeCanvasItem *item)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (item);
	GList *l;
	int height = 0;
	
	GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->realize (item);

	for (l = etgc->children; l; l = l->next){
		GnomeCanvasItem *child = l->data;

		printf ("During realization for child %p -> %d\n", child, height);
		gnome_canvas_item_set (
			child,
			"y", (double) height,
			NULL);

		height += child->y2 - child->y1;
	}
}

static void
etgc_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (item);

	GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->update (item, affine, clip_path, flags);

	if (etgc->need_resize) {
		
		if (!etgc->transparent) {
			int current_width, current_height;

			etgc_dim (etgc, &current_width, &current_height);
		
			if ((current_height != etgc->height) || (current_width != etgc->width)){
				etgc->width = current_width;
				etgc->height = current_height;
			
				gnome_canvas_item_set (
						       etgc->rect,
						       "x1", 0.0,
						       "y1", 0.0,
						       "x2", (double) etgc->width,
						       "y2", (double) etgc->height,
						       NULL);
			}
		}
		etgc->need_resize = FALSE;
	}
}
#endif

static int
etgc_event (GnomeCanvasItem *item, GdkEvent *event)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(item);
	gboolean return_val = TRUE;
	gboolean change_focus = FALSE;
	gboolean use_col = FALSE;
	gint start_col = 0;
	gint old_col;
	EFocus direction = E_FOCUS_START;

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_Tab || 
		    event->key.keyval == GDK_KP_Tab || 
		    event->key.keyval == GDK_ISO_Left_Tab) {
			change_focus = TRUE;
			use_col      = TRUE;
			start_col    = (event->key.state & GDK_SHIFT_MASK) ? -1 : 0;
			direction    = (event->key.state & GDK_SHIFT_MASK) ? E_FOCUS_END : E_FOCUS_START;
		} else if (event->key.keyval == GDK_Left ||
			   event->key.keyval == GDK_KP_Left) {
			change_focus = TRUE;
			use_col      = TRUE;
			start_col    = -1;
			direction    = E_FOCUS_END;
		} else if (event->key.keyval == GDK_Right ||
			   event->key.keyval == GDK_KP_Right) {
			change_focus = TRUE;
			use_col   = TRUE;
			start_col = 0;
			direction = E_FOCUS_START;
		} else if (event->key.keyval == GDK_Down ||
			   event->key.keyval == GDK_KP_Down) {
			change_focus = TRUE;
			use_col      = FALSE;
			direction    = E_FOCUS_START;
		} else if (event->key.keyval == GDK_Up ||
			   event->key.keyval == GDK_KP_Up) {
			change_focus = TRUE;
			use_col      = FALSE;
			direction    = E_FOCUS_END;
		} else if (event->key.keyval == GDK_Return ||
			   event->key.keyval == GDK_KP_Enter) {
			change_focus = TRUE;
			use_col      = FALSE;
			direction    = E_FOCUS_START;
		}
		if ( change_focus ) {		
			GList *list;
			for (list = etgc->children; list; list = list->next) {
				ETableGroupContainerChildNode *child_node;
				ETableGroup                   *child;

				child_node = (ETableGroupContainerChildNode *)list->data;
				child      = child_node->child;

				if (e_table_group_get_focus (child)) {
					old_col = e_table_group_get_focus_column (child);
					if (old_col == -1)
						old_col = 0;
					if (start_col == -1)
						start_col = e_table_header_count (e_table_group_get_header(child)) - 1;

					if (direction == E_FOCUS_END)
						list = list->prev;
					else
						list = list->next;
					
					if (list) {
						child_node = (ETableGroupContainerChildNode *)list->data;
						child = child_node->child;
						if (use_col)
							e_table_group_set_focus (child, direction, start_col);
						else
							e_table_group_set_focus (child, direction, old_col);
						return 1;
					} else {
						return 0;
					}
				}
			}
		}
		return_val = FALSE;
	default:
		return_val = FALSE;
	}
	if (return_val == FALSE) {
		if (GNOME_CANVAS_ITEM_CLASS(etgc_parent_class)->event)
			return GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->event (item, event);
	}
	return return_val;
	
}

/* Realize handler for the text item */
static void
etgc_realize (GnomeCanvasItem *item)
{
	ETableGroupContainer *etgc;

	if (GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->realize)
		(* GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->realize) (item);

	etgc = E_TABLE_GROUP_CONTAINER (item);

	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (etgc));
}

/* Unrealize handler for the etgc item */
static void
etgc_unrealize (GnomeCanvasItem *item)
{
	ETableGroupContainer *etgc;

	etgc = E_TABLE_GROUP_CONTAINER (item);
	
	if (GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->unrealize)
		(* GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->unrealize) (item);
}

static void
compute_text (ETableGroupContainer *etgc, ETableGroupContainerChildNode *child_node)
{
	/* FIXME : What a hack, eh? */
	gchar *text = g_strdup_printf ("%s : %s (%d item%s)",
				       etgc->ecol->text,
				       (gchar *)child_node->key,
				       (gint) child_node->count,
				       child_node->count == 1 ? "" : "s" );
	gnome_canvas_item_set (child_node->text, 
			       "text", text,
			       NULL);
	g_free (text);
}

static void
child_row_selection (ETableGroup *etg, int row, gboolean selected,
		     ETableGroupContainer *etgc)
{
	e_table_group_row_selection (E_TABLE_GROUP (etgc), row, selected);
}

static void
etgc_add (ETableGroup *etg, gint row)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	void *val = e_table_model_value_at (etg->model, etgc->ecol->col_idx, row);
	GCompareFunc comp = etgc->ecol->compare;
	GList *list = etgc->children;
	ETableGroup *child;
	ETableGroupContainerChildNode *child_node;
	int i = 0;
	for ( ; list; list = g_list_next(list), i++ ) {
		int comp_val;
		child_node = (ETableGroupContainerChildNode *)(list->data);
		comp_val = (*comp)(child_node->key, val);
		if (comp_val == 0) {
			child = child_node->child;
			child_node->count ++;
			e_table_group_add (child, row);
			compute_text (etgc, child_node);
			return;
		}
		if ((comp_val > 0 && etgc->ascending) ||
		    (comp_val < 0 && (!etgc->ascending)))
			break;
	}
	child_node = g_new (ETableGroupContainerChildNode, 1);
	child_node->rect = gnome_canvas_item_new (GNOME_CANVAS_GROUP (etgc),
						  gnome_canvas_rect_get_type (),
						  "fill_color", "grey70",
						  "outline_color", "grey50",
						  NULL);
	child_node->text = gnome_canvas_item_new (GNOME_CANVAS_GROUP (etgc),
						  e_text_get_type (),
						  "font_gdk", etgc->font,
						  "anchor", GTK_ANCHOR_SW,
						  "x", (double) 0,
						  "y", (double) 0,
						  "fill_color", "black",
						  NULL);
	child = e_table_group_new (GNOME_CANVAS_GROUP (etgc), etg->full_header,
				   etg->header, etg->model, etgc->child_rules);
	gtk_signal_connect (GTK_OBJECT (child), "row_selection",
			    GTK_SIGNAL_FUNC (child_row_selection), etgc);
	child_node->child = child;
	child_node->key = e_table_model_duplicate_value (etg->model, etgc->ecol->col_idx, val);
	child_node->count = 1;
	e_table_group_add (child, row);

	if (list)
		etgc->children = g_list_insert (etgc->children, child_node, i);
	else
		etgc->children = g_list_append (etgc->children, child_node);

	compute_text (etgc, child_node);
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (etgc));
}

static gboolean
etgc_remove (ETableGroup *etg, gint row)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	GList *list;

	for (list = etgc->children ; list; list = g_list_next (list)) {
		ETableGroupContainerChildNode *child_node = list->data;
		ETableGroup                   *child = child_node->child;

		if (e_table_group_remove (child, row)) {
			child_node->count --;
			if (child_node->count == 0) {
				e_table_group_container_child_node_free (etgc, child_node);
				etgc->children = g_list_remove (etgc->children, child_node);
				g_free (child_node);
			} else
				compute_text (etgc, child_node);

			e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (etgc));

			return TRUE;
		}
	}
	return FALSE;
}

static void
etgc_increment (ETableGroup *etg, gint position, gint amount)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	GList *list = etgc->children;

	for (list = etgc->children ; list; list = g_list_next (list))
		e_table_group_increment (((ETableGroupContainerChildNode *)list->data)->child,
					 position, amount);
}

static void
etgc_set_focus (ETableGroup *etg, EFocus direction, gint view_col)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	if (etgc->children) {
		if (direction == E_FOCUS_END)
			e_table_group_set_focus (((ETableGroupContainerChildNode *)g_list_last (etgc->children)->data)->child,
						 direction, view_col);
		else
			e_table_group_set_focus (((ETableGroupContainerChildNode *)etgc->children->data)->child,
						 direction, view_col);
	}
}

static gint
etgc_get_focus_column (ETableGroup *etg)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	if (etgc->children) {
		GList *list;
		for (list = etgc->children; list; list = list->next) {
			ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *)list->data;
			ETableGroup *child = child_node->child;
			if (e_table_group_get_focus(child)) {
				return e_table_group_get_focus_column(child);
			}
		}
	}
	return 0;
}

static void etgc_thaw (ETableGroup *etg)
{
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etg));
}

static void
etgc_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableGroup *etg = E_TABLE_GROUP (object);

	switch (arg_id) {
	case ARG_FROZEN:
		if ( GTK_VALUE_BOOL (*arg) )
			etg->frozen = TRUE;
		else {
			etg->frozen = FALSE;
			etgc_thaw(etg);
		}
		break;
	case ARG_WIDTH:
		if ( E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->set_width )
			E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->set_width(etg, GTK_VALUE_DOUBLE (*arg));
		break;
	default:
		break;
	}
}

static void
etgc_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableGroup *etg = E_TABLE_GROUP (object);

	switch (arg_id) {
	case ARG_FROZEN:
		GTK_VALUE_BOOL (*arg) = etg->frozen;
		break;
	case ARG_HEIGHT:
		if ( E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->get_height )
			GTK_VALUE_DOUBLE (*arg) = E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->get_height(etg);
		else
			arg->type = GTK_TYPE_INVALID;
		break;
	case ARG_WIDTH:	
		if ( E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->get_width )
			GTK_VALUE_DOUBLE (*arg) = E_TABLE_GROUP_CLASS(GTK_OBJECT(etg)->klass)->get_width(etg);
		else
			arg->type = GTK_TYPE_INVALID;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void etgc_set_width (ETableGroup *etg, gdouble width)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (etg);
	GList *list = etgc->children;
	etgc->width = width;

	for ( ; list; list = g_list_next(list) ) {
		gdouble child_width = width - GROUP_INDENT;
		ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *)list->data;
		gtk_object_set(GTK_OBJECT(child_node->child),
			       "width", child_width,
			       NULL);

		gnome_canvas_item_set(GNOME_CANVAS_ITEM(child_node->rect),
				      "x1", (double) 0,
				      "x2", (double) etgc->width,
				      NULL);
	}
}

static gdouble etgc_get_width (ETableGroup *etg)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (etg);
	return etgc->width;
}

static gdouble etgc_get_height (ETableGroup *etg)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (etg);
	return etgc->height;
}

static void
etgc_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;
	ETableGroupClass *e_group_class = E_TABLE_GROUP_CLASS( object_class );

	object_class->destroy = etgc_destroy;
	object_class->set_arg = etgc_set_arg;
	object_class->get_arg = etgc_get_arg;

	item_class->event = etgc_event;
	item_class->realize = etgc_realize;
	item_class->unrealize = etgc_unrealize;

	etgc_parent_class = gtk_type_class (PARENT_TYPE);

	e_group_class->add = etgc_add;
	e_group_class->remove = etgc_remove;
	e_group_class->increment = etgc_increment;
	e_group_class->set_focus = etgc_set_focus;
	e_group_class->get_focus_column = etgc_get_focus_column;
	e_group_class->thaw = etgc_thaw;

	e_group_class->get_width = etgc_get_width;
	e_group_class->set_width = etgc_set_width;
	e_group_class->get_height = etgc_get_height;

	gtk_object_add_arg_type ("ETableGroupContainer::height", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READABLE, ARG_HEIGHT);
	gtk_object_add_arg_type ("ETableGroupContainer::width", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("ETableGroupContainer::frozen", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_FROZEN);
}

static void
etgc_reflow (GnomeCanvasItem *item, gint flags)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(item);
	gboolean frozen;
	gtk_object_get(GTK_OBJECT(etgc),
		       "frozen", &frozen,
		       NULL);
	if ( frozen ) {
		etgc->idle = 0;
		return;
	}
	if ( GTK_OBJECT_FLAGS( etgc ) & GNOME_CANVAS_ITEM_REALIZED ) {
		gdouble old_height;
		
		old_height = etgc->height;
		if ( etgc->children == NULL ) {
		} else {
			GList *list;
			gdouble extra_height;
			gdouble running_height;
			gdouble item_height = 0;
			
			extra_height = 0;
			if (etgc->font)
				extra_height += etgc->font->ascent + etgc->font->descent + BUTTON_PADDING * 2;
			
			extra_height = MAX(extra_height, BUTTON_HEIGHT + BUTTON_PADDING * 2);
				
			running_height = extra_height;
			
			list = etgc->children;
			for ( ; list; list = g_list_next(list) ) {
				ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *) list->data;
				ETableGroup *child = child_node->child;
				gtk_object_get( GTK_OBJECT(child),
						"height", &item_height,
						NULL );
				
				e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(child_node->text),
							    GROUP_INDENT,
							    running_height - BUTTON_PADDING);
				
				e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(child),
							    GROUP_INDENT,
							    running_height);
				
				gnome_canvas_item_set(GNOME_CANVAS_ITEM(child_node->rect),
						      "x1", (double) 0,
						      "x2", (double) etgc->width,
						      "y1", (double) running_height - extra_height,
						      "y2", (double) running_height + item_height,
						      NULL);
				
				running_height += item_height + extra_height;
			}
			running_height -= extra_height;
			if ( running_height != old_height) {
				etgc->height = running_height;
				e_canvas_item_request_parent_reflow(item);
			}
		}
	}
	etgc->idle = 0;
}

static void
etgc_init (GtkObject *object)
{
	ETableGroupContainer *container = E_TABLE_GROUP_CONTAINER(object);
	container->children = FALSE;
	
	e_canvas_item_set_reflow_callback(GNOME_CANVAS_ITEM(object), etgc_reflow);
}

E_MAKE_TYPE (e_table_group_container, "ETableGroupContainer", ETableGroupContainer, etgc_class_init, etgc_init, PARENT_TYPE);




