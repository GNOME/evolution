/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-click-to-add.c: A canvas item based view of the ETableColumn.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-canvas-util.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "e-table-header.h"
#include "e-table-click-to-add.h"
#include "e-table-defines.h"
#include "e-table-one.h"
#include "widgets/e-text/e-text.h"
#include "e-util/e-canvas.h"

enum {
	ROW_SELECTION,
	LAST_SIGNAL
};

static gint etcta_signals [LAST_SIGNAL] = { 0, };

#define PARENT_OBJECT_TYPE gnome_canvas_group_get_type ()

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

static GnomeCanvasGroupClass *etcta_parent_class;

enum {
	ARG_0,
	ARG_HEADER,
	ARG_MODEL,
	ARG_MESSAGE,
	ARG_WIDTH,
	ARG_HEIGHT,
};

static void
etcta_row_selection (GtkObject *object, gint row, gboolean selected, ETableClickToAdd *etcta)
{
	gtk_signal_emit (GTK_OBJECT (etcta),
			 etcta_signals [ROW_SELECTION],
			 row, selected);
}

static void
etcta_add_table_header (ETableClickToAdd *etcta, ETableHeader *header)
{
	etcta->eth = header;
	if (etcta->eth)
		gtk_object_ref (GTK_OBJECT (etcta->eth));
	if (etcta->row)
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(etcta->row),
				      "ETableHeader", header,
				      NULL);
}

static void
etcta_drop_table_header (ETableClickToAdd *etcta)
{
	GtkObject *header;
	
	if (!etcta->eth)
		return;

	header = GTK_OBJECT (etcta->eth);

	gtk_object_unref (header);
	etcta->eth = NULL;
}

static void
etcta_add_one (ETableClickToAdd *etcta, ETableModel *one)
{
	etcta->one = one;
	if (etcta->one)
		gtk_object_ref (GTK_OBJECT(etcta->one));
	if (etcta->row)
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(etcta->row),
				      "ETableModel", one,
				      NULL);
}

static void
etcta_drop_one (ETableClickToAdd *etcta)
{
	if (!etcta->one)
		return;
	gtk_object_unref (GTK_OBJECT(etcta->one));
	etcta->one = NULL;
}

static void
etcta_add_model (ETableClickToAdd *etcta, ETableModel *model)
{
	etcta->model = model;
	if (etcta->model)
		gtk_object_ref (GTK_OBJECT(etcta->model));
}

static void
etcta_drop_model (ETableClickToAdd *etcta)
{
	etcta_drop_one (etcta);
	if (!etcta->model)
		return;
	gtk_object_unref (GTK_OBJECT(etcta->model));
	etcta->model = NULL;
}

static void
etcta_add_message (ETableClickToAdd *etcta, char *message)
{
	etcta->message = g_strdup(message);
}

static void
etcta_drop_message (ETableClickToAdd *etcta)
{
	g_free(etcta->message);
	etcta->message = NULL;
}


static void
etcta_destroy (GtkObject *object){
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (object);

	etcta_drop_table_header (etcta);
	etcta_drop_model (etcta);
	etcta_drop_message (etcta);

	if (GTK_OBJECT_CLASS (etcta_parent_class)->destroy)
		(*GTK_OBJECT_CLASS (etcta_parent_class)->destroy) (object);
}

static void
etcta_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	ETableClickToAdd *etcta;

	item = GNOME_CANVAS_ITEM (o);
	etcta = E_TABLE_CLICK_TO_ADD (o);

	switch (arg_id){
	case ARG_HEADER:
		etcta_drop_table_header (etcta);
		etcta_add_table_header (etcta, E_TABLE_HEADER(GTK_VALUE_OBJECT (*arg)));
		break;
	case ARG_MODEL:
		etcta_drop_model (etcta);
		etcta_add_model (etcta, E_TABLE_MODEL(GTK_VALUE_OBJECT (*arg)));
		break;
	case ARG_MESSAGE:
		etcta_drop_message (etcta);
		etcta_add_message (etcta, GTK_VALUE_STRING (*arg));
		break;
	case ARG_WIDTH:
		etcta->width = GTK_VALUE_DOUBLE (*arg);
		if (etcta->row)
			gnome_canvas_item_set(etcta->row,
					      "minimum_width", etcta->width,
					      NULL);
		if (etcta->text)
			gnome_canvas_item_set(etcta->text,
					      "width", etcta->width,
					      NULL);
		break;
	}
	gnome_canvas_item_request_update(item);
}

static void
etcta_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableClickToAdd *etcta;

	etcta = E_TABLE_CLICK_TO_ADD (o);

	switch (arg_id){
	case ARG_HEADER:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(etcta->eth);
		break;
	case ARG_MODEL:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(etcta->model);
		break;
	case ARG_MESSAGE:
		GTK_VALUE_STRING (*arg) = g_strdup(etcta->message);
		break;
	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = etcta->width;
		break;
	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = etcta->height;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
etcta_realize (GnomeCanvasItem *item)
{
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (item);
	etcta->text = gnome_canvas_item_new(GNOME_CANVAS_GROUP(item),
					    e_text_get_type(),
					    "text", etcta->message ? etcta->message : "",
					    "anchor", GTK_ANCHOR_NW,
					    "width", etcta->width,
					    NULL);

	if (GNOME_CANVAS_ITEM_CLASS (etcta_parent_class)->realize)
		(*GNOME_CANVAS_ITEM_CLASS (etcta_parent_class)->realize)(item);
}

static void
etcta_unrealize (GnomeCanvasItem *item)
{
	if (GNOME_CANVAS_ITEM_CLASS (etcta_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (etcta_parent_class)->unrealize)(item);
}

static double
etcta_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
	    GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}

/*
 * Handles the events on the ETableClickToAdd, particularly it creates the ETableItem and passes in some events.
 */
static int
etcta_event (GnomeCanvasItem *item, GdkEvent *e)
{
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (item);
	int ret_val = TRUE;
	
	switch (e->type){
	case GDK_BUTTON_PRESS:
		if (etcta->text) {
			gtk_object_destroy(GTK_OBJECT(etcta->text));
			etcta->text = NULL;
		}
		if (!etcta->row) {
			ETableModel *one;

			one = e_table_one_new(etcta->model);
			etcta_add_one (etcta, one);
			gtk_object_unref(GTK_OBJECT(one));

			etcta->row = gnome_canvas_item_new(GNOME_CANVAS_GROUP(item),
							   e_table_item_get_type(),
							   "ETableHeader", etcta->eth,
							   "ETableModel", etcta->one,
							   "minimum_width", etcta->width,
							   "drawgrid", TRUE,
							   NULL);

			gtk_signal_connect(GTK_OBJECT(etcta->row), "row_selection",
					   GTK_SIGNAL_FUNC(etcta_row_selection), etcta);
		}
		/* Fall through.  No break; */
	case GDK_BUTTON_RELEASE:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		if (etcta->row) {
			gnome_canvas_item_i2w (item, &e->button.x, &e->button.y);
			gnome_canvas_item_w2i (etcta->row, &e->button.x, &e->button.y);
			gtk_signal_emit_by_name(GTK_OBJECT(etcta->row), "event", e, &ret_val);
			gnome_canvas_item_i2w (etcta->row, &e->button.x, &e->button.y);
			gnome_canvas_item_w2i (item, &e->button.x, &e->button.y);
		}
		break;
		
	default:
		return FALSE;
	}
	return TRUE;
}

static void
etcta_reflow (GnomeCanvasItem *item, int flags)
{
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (item);
	
	if (etcta->text) {
		gtk_object_get(GTK_OBJECT(etcta->text),
			       "height", &etcta->height,
			       NULL);
	}
	if (etcta->row) {
		gtk_object_get(GTK_OBJECT(etcta->row),
			       "height", &etcta->height,
			       NULL);
	}
	e_canvas_item_request_parent_reflow(item);
}

static void
etcta_class_init (ETableClickToAddClass *klass)
{
	GnomeCanvasItemClass *item_class = GNOME_CANVAS_ITEM_CLASS(klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

	etcta_parent_class = gtk_type_class (PARENT_OBJECT_TYPE);

	klass->row_selection = NULL;

	object_class->destroy = etcta_destroy;
	object_class->set_arg = etcta_set_arg;
	object_class->get_arg = etcta_get_arg;

	item_class->realize     = etcta_realize;
	item_class->unrealize   = etcta_unrealize;
	item_class->point       = etcta_point;
	item_class->event       = etcta_event;

	gtk_object_add_arg_type ("ETableClickToAdd::header", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_HEADER);
	gtk_object_add_arg_type ("ETableClickToAdd::model", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_MODEL);
	gtk_object_add_arg_type ("ETableClickToAdd::message", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_MESSAGE);
	gtk_object_add_arg_type ("ETableClickToAdd::width", GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("ETableClickToAdd::height", GTK_TYPE_DOUBLE,
				 GTK_ARG_READABLE, ARG_HEIGHT);

	etcta_signals [ROW_SELECTION] =
		gtk_signal_new ("row_selection",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableClickToAddClass, row_selection),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, etcta_signals, LAST_SIGNAL);
}

static void
etcta_init (GnomeCanvasItem *item)
{
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (item);

	etcta->one = NULL;
	etcta->model = NULL;
	etcta->eth = NULL;

	etcta->message = NULL;

	etcta->row = NULL;
	etcta->text = NULL;
	etcta->rect = NULL;

	e_canvas_item_set_reflow_callback(item, etcta_reflow);
}

GtkType
e_table_click_to_add_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableClickToAdd",
			sizeof (ETableClickToAdd),
			sizeof (ETableClickToAddClass),
			(GtkClassInitFunc) etcta_class_init,
			(GtkObjectInitFunc) etcta_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_OBJECT_TYPE, &info);
	}

	return type;
}

void
e_table_click_to_add_commit (ETableClickToAdd *etcta)
{
		if (etcta->row) {
			e_table_one_commit(E_TABLE_ONE(etcta->one));
			gtk_object_destroy(GTK_OBJECT(etcta->row));
			etcta_drop_one (etcta);
			etcta->row = NULL;
		}
		if (!etcta->text) {
			etcta->text = gnome_canvas_item_new(GNOME_CANVAS_GROUP(etcta),
							    e_text_get_type(),
							    "text", etcta->message ? etcta->message : "",
							    "anchor", GTK_ANCHOR_NW,
							    "width", etcta->width,
							    NULL);
		}
}
