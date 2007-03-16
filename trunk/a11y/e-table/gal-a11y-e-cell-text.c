/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>

#include <string.h>

#include <atk/atk.h>

#include "a11y/gal-a11y-util.h"
#include "table/e-cell-text.h"
#include "e-util/e-i18n.h"

#include "gal-a11y-e-cell-text.h"

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yECellTextClass))
static AtkObjectClass *parent_class;
#define PARENT_TYPE (gal_a11y_e_cell_get_type ())

/* Static functions */
static void
ect_dispose (GObject *object)
{
	GObjectClass *g_class;
	GalA11yECell *gaec = GAL_A11Y_E_CELL (object);
	GalA11yECellText *gaet = GAL_A11Y_E_CELL_TEXT (object);

	if (gaet->inserted_id != 0) {
		ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);

		if (ect) {
			g_signal_handler_disconnect (ect, gaet->inserted_id);
			g_signal_handler_disconnect (ect, gaet->deleted_id);
		}

		gaet->inserted_id = 0;
		gaet->deleted_id = 0;
	}

	g_class = (GObjectClass *)parent_class;
	if (g_class->dispose)
		g_class->dispose (object);

}

static gboolean
ect_check (gpointer a11y)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (a11y);
	ETableItem *item = gaec->item;

	g_return_val_if_fail ((gaec->item != NULL), FALSE);
	g_return_val_if_fail ((gaec->cell_view != NULL), FALSE);
	g_return_val_if_fail ((gaec->cell_view->ecell != NULL), FALSE);

	if (atk_state_set_contains_state (gaec->state_set, ATK_STATE_DEFUNCT))
		return FALSE;

	if (gaec->row < 0 || gaec->row >= item->rows
		|| gaec->view_col <0 || gaec->view_col >= item->cols
		|| gaec->model_col <0 || gaec->model_col >= e_table_model_column_count (item->table_model))
		return FALSE;

	if (!E_IS_CELL_TEXT (gaec->cell_view->ecell))
		return FALSE;

	return TRUE;
}

static G_CONST_RETURN gchar*
ect_get_name (AtkObject * a11y)
{
	GalA11yECell *gaec;
	char *name;

	if (!ect_check (a11y))
		return NULL;

	gaec = GAL_A11Y_E_CELL (a11y);
	name = e_cell_text_get_text_by_view (gaec->cell_view, gaec->model_col, gaec->row);
	if (name != NULL) {
		ATK_OBJECT_CLASS (parent_class)->set_name (a11y, name);
		g_free (name);
	}

	if (a11y->name != NULL && strcmp (a11y->name, "")) {
		return a11y->name;
	} else {
		return parent_class->get_name (a11y);
	}
}

static gchar *
ect_get_text (AtkText *text,
	      gint start_offset,
	      gint end_offset)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	gchar *full_text;
	gchar *ret_val;

	if (!ect_check (text))
		return NULL;

	full_text = e_cell_text_get_text_by_view (gaec->cell_view, gaec->model_col, gaec->row);

	if (end_offset == -1)
		end_offset = strlen (full_text);
	else
		end_offset = g_utf8_offset_to_pointer (full_text, end_offset) - full_text;

	start_offset = g_utf8_offset_to_pointer (full_text, start_offset) - full_text;

	ret_val = g_strndup (full_text + start_offset, end_offset - start_offset);

	g_free (full_text);

	return ret_val;
}

static gchar *
ect_get_text_after_offset (AtkText *text,
			   gint offset,
			   AtkTextBoundary boundary_type,
			   gint *start_offset,
			   gint *end_offset)
{
	/* Unimplemented */
	return NULL;
}

static gchar *
ect_get_text_at_offset (AtkText *text,
			gint offset,
			AtkTextBoundary boundary_type,
			gint *start_offset,
			gint *end_offset)
{
	/* Unimplemented */
	return NULL;
}

static gunichar
ect_get_character_at_offset (AtkText *text,
			     gint offset)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	gunichar ret_val;
	gchar *at_offset;

	if (!ect_check (text))
		return -1;

	gchar *full_text = e_cell_text_get_text_by_view (gaec->cell_view, gaec->model_col, gaec->row);
	at_offset = g_utf8_offset_to_pointer (full_text, offset);
	ret_val = g_utf8_get_char_validated (at_offset, -1);
	g_free (full_text);

	return ret_val;
}


static gchar*
ect_get_text_before_offset (AtkText *text,
			    gint offset,
			    AtkTextBoundary boundary_type,
			    gint *start_offset,
			    gint *end_offset)
{
	/* Unimplemented */
	return NULL;
}


static gint
ect_get_caret_offset (AtkText *text)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	gint start, end;

	if (!ect_check (text))
		return -1;

	if (e_cell_text_get_selection (gaec->cell_view,
				       gaec->view_col, gaec->row,
				       &start, &end)) {
		gchar *full_text = e_cell_text_get_text_by_view (gaec->cell_view, gaec->model_col, gaec->row);
		end = g_utf8_pointer_to_offset (full_text, full_text + end);
		g_free (full_text);
		
		return end;
	}
	else
		return -1;
}

static AtkAttributeSet*
ect_get_run_attributes (AtkText *text,
			gint offset,
			gint *start_offset,
			gint *end_offset)
{
	/* Unimplemented */
	return NULL;
}


static AtkAttributeSet*
ect_get_default_attributes (AtkText *text)
{
	/* Unimplemented */
	return NULL;
}


static void
ect_get_character_extents (AtkText *text,
			   gint offset,
			   gint *x,
			   gint *y,
			   gint *width,
			   gint *height,
			   AtkCoordType coords)
{
	/* Unimplemented */
}


static gint
ect_get_character_count (AtkText *text)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	gint ret_val;

	if (!ect_check (text))
		return -1;

	gchar *full_text = e_cell_text_get_text_by_view (gaec->cell_view, gaec->model_col, gaec->row);

	ret_val = g_utf8_strlen (full_text, -1);
	g_free (full_text);
	return ret_val;
}


static gint
ect_get_offset_at_point (AtkText *text,
			 gint x,
			 gint y,
			 AtkCoordType coords)
{
	/* Unimplemented */
	return 0;
}


static gint 
ect_get_n_selections (AtkText *text)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	gint selection_start, selection_end;

	if (!ect_check (text))
		return 0;

	if (e_cell_text_get_selection (gaec->cell_view,
				       gaec->view_col, gaec->row,
				       &selection_start,
				       &selection_end)
	    && selection_start != selection_end)
		return 1;
	return 0;
}


static gchar*
ect_get_selection (AtkText *text,
		   gint selection_num,
		   gint *start_offset,
		   gint *end_offset)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	gchar *ret_val;
	gint selection_start, selection_end;

	if (selection_num == 0
	    && e_cell_text_get_selection (gaec->cell_view,
					  gaec->view_col, gaec->row,
					  &selection_start,
					  &selection_end)
	    && selection_start != selection_end) {
		gint real_start, real_end, len;
		gchar *full_text = e_cell_text_get_text_by_view (gaec->cell_view, gaec->model_col, gaec->row);
		len = strlen (full_text);
		real_start = MIN (selection_start, selection_end);
		real_end   = MAX (selection_start, selection_end);
		real_start = MIN (MAX (0, real_start), len);
		real_end   = MIN (MAX (0, real_end), len);

		ret_val = g_strndup (full_text + real_start, real_end - real_start);

		real_start = g_utf8_pointer_to_offset (full_text, full_text + real_start);
		real_end   = g_utf8_pointer_to_offset (full_text, full_text + real_end);

		if (start_offset)
			*start_offset = real_start;
		if (end_offset)
			*end_offset = real_end;
		g_free (full_text);
	} else {
		if (start_offset)
			*start_offset = 0;
		if (end_offset)
			*end_offset = 0;
		ret_val = NULL;
	}

	return ret_val;
}


static gboolean
ect_add_selection (AtkText *text,
		   gint start_offset,
		   gint end_offset)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);

	if (start_offset != end_offset) {
		gint real_start, real_end, len;
		gchar *full_text =
			e_cell_text_get_text_by_view (gaec->cell_view, gaec->model_col, gaec->row);

		len = g_utf8_strlen (full_text, -1);
		if (end_offset == -1)
			end_offset = len;

		real_start = MIN (start_offset, end_offset);
		real_end   = MAX (start_offset, end_offset);
		
		real_start = MIN (MAX (0, real_start), len);
		real_end   = MIN (MAX (0, real_end), len);

		real_start = g_utf8_offset_to_pointer (full_text, real_start) - full_text;
		real_end   = g_utf8_offset_to_pointer (full_text, real_end) - full_text;
		g_free (full_text);

		if (e_cell_text_set_selection (gaec->cell_view,
					       gaec->view_col, gaec->row,
					       real_start, real_end)) {
			g_signal_emit_by_name (ATK_OBJECT(text), "text_selection_changed");
			return TRUE;
		}
	}

	return FALSE;
}


static gboolean
ect_remove_selection (AtkText *text,
		      gint selection_num)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	gint selection_start, selection_end;

	if (selection_num == 0
	    && e_cell_text_get_selection (gaec->cell_view,
					  gaec->view_col, gaec->row,
					  &selection_start,
					  &selection_end)
	    && selection_start != selection_end 
	    && e_cell_text_set_selection (gaec->cell_view,
					  gaec->view_col, gaec->row,
					  selection_end, selection_end)) {
		g_signal_emit_by_name (ATK_OBJECT(text), "text_selection_changed");
		return TRUE;
	}
	else
		return FALSE;
}


static gboolean
ect_set_selection (AtkText *text,
		   gint selection_num,
		   gint start_offset,
		   gint end_offset)
{
	if (selection_num == 0) {
		atk_text_add_selection (text, start_offset, end_offset);
		return TRUE;
	}
	else
		return FALSE;
}


static gboolean
ect_set_caret_offset (AtkText *text,
		      gint offset)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	gchar *full_text;
	gint len;

	full_text = e_cell_text_get_text_by_view (gaec->cell_view, gaec->model_col, gaec->row);

	len = g_utf8_strlen (full_text, -1);
	if (offset == -1)
		offset = len;
	else
		offset = MIN (MAX (0, offset), len);
	
	offset = g_utf8_offset_to_pointer (full_text, offset) - full_text;

	g_free (full_text);

	return e_cell_text_set_selection (gaec->cell_view,
					  gaec->view_col, gaec->row,
					  offset, offset);
}

static gboolean
ect_set_run_attributes (AtkEditableText *text,
			AtkAttributeSet *attrib_set,
			gint start_offset,
			gint end_offset)
{
	/* Unimplemented */
	return FALSE;
}

static void
ect_set_text_contents (AtkEditableText *text,
		       const gchar *string)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);

	e_cell_text_set_value (ect, gaec->item->table_model, gaec->model_col, gaec->row, string);
	e_table_item_enter_edit (gaec->item, gaec->view_col, gaec->row);
}

static void
ect_insert_text (AtkEditableText *text,
		 const gchar *string,
		 gint length,
		 gint *position)
{
	/* Utf8 unimplemented */
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);

	gchar *full_text = e_cell_text_get_text_by_view (gaec->cell_view, gaec->model_col, gaec->row);
	gchar *result = g_strdup_printf ("%.*s%.*s%s", *position, full_text, length, string, full_text + *position);

	e_cell_text_set_value (ect, gaec->item->table_model, gaec->model_col, gaec->row, result);

	*position += length;

	g_free (result);
	g_free (full_text);
}

static void
ect_copy_text (AtkEditableText *text,
	       gint start_pos,
	       gint end_pos)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	if (start_pos != end_pos
	    && atk_text_set_selection (ATK_TEXT (text), 0, start_pos, end_pos))
		e_cell_text_copy_clipboard (gaec->cell_view,
					    gaec->view_col, gaec->row);
}

static void
ect_delete_text (AtkEditableText *text,
		 gint start_pos,
		 gint end_pos)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	if (start_pos != end_pos
	    && atk_text_set_selection (ATK_TEXT (text), 0, start_pos, end_pos))
		e_cell_text_delete_selection (gaec->cell_view,
					      gaec->view_col, gaec->row);
}

static void
ect_cut_text (AtkEditableText *text,
	      gint start_pos,
	      gint end_pos)
{
	ect_copy_text (text, start_pos, end_pos);
	ect_delete_text (text, start_pos, end_pos);
}

static void
ect_paste_text (AtkEditableText *text,
		gint position)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);

	e_table_item_enter_edit (gaec->item, gaec->view_col, gaec->row);

	if (atk_text_set_caret_offset (ATK_TEXT (text), position))
		e_cell_text_paste_clipboard (gaec->cell_view,
					     gaec->view_col, gaec->row);
}

static void
ect_do_action_edit (AtkAction *action)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (action);
	ETableModel *e_table_model = a11y->item->table_model;

	if (e_table_model_is_cell_editable(e_table_model, a11y->model_col, a11y->row)) {
		e_table_item_enter_edit (a11y->item, a11y->view_col, a11y->row);
	}
}

/* text signal handlers */
static void
ect_text_inserted_cb (ECellText *text, ECellView *cell_view, int pos, int len, int row, int model_col, gpointer data)
{
	GalA11yECellText *gaet;
	GalA11yECell *gaec;

	if (!ect_check (data))
		return;
	gaet = GAL_A11Y_E_CELL_TEXT (data);
	gaec = GAL_A11Y_E_CELL (data);

	if (cell_view == gaec->cell_view && row == gaec->row && model_col == gaec->model_col) {
		g_signal_emit_by_name (gaet, "text_changed::insert", pos, len);

	}
}

static void
ect_text_deleted_cb (ECellText *text, ECellView *cell_view, int pos, int len, int row, int model_col, gpointer data)
{
	GalA11yECellText *gaet;
	GalA11yECell *gaec;
	if (!ect_check (data))
		return;
	gaet = GAL_A11Y_E_CELL_TEXT (data);
	gaec = GAL_A11Y_E_CELL (data);
	if (cell_view == gaec->cell_view && row == gaec->row && model_col == gaec->model_col) {
		g_signal_emit_by_name (gaet, "text_changed::delete", pos, len);
	 }
}

static void
ect_atk_text_iface_init (AtkTextIface *iface)
{
	iface->get_text                = ect_get_text;
	iface->get_text_after_offset   = ect_get_text_after_offset;
	iface->get_text_at_offset      = ect_get_text_at_offset;
	iface->get_character_at_offset = ect_get_character_at_offset;
	iface->get_text_before_offset  = ect_get_text_before_offset;
	iface->get_caret_offset        = ect_get_caret_offset;
	iface->get_run_attributes      = ect_get_run_attributes;
	iface->get_default_attributes  = ect_get_default_attributes;
	iface->get_character_extents   = ect_get_character_extents;
	iface->get_character_count     = ect_get_character_count;
	iface->get_offset_at_point     = ect_get_offset_at_point;
	iface->get_n_selections        = ect_get_n_selections;
	iface->get_selection           = ect_get_selection;
	iface->add_selection           = ect_add_selection;
	iface->remove_selection        = ect_remove_selection;
	iface->set_selection           = ect_set_selection;
	iface->set_caret_offset        = ect_set_caret_offset;
}

static void
ect_atk_editable_text_iface_init (AtkEditableTextIface *iface)
{
	iface->set_run_attributes = ect_set_run_attributes;
	iface->set_text_contents  = ect_set_text_contents;
	iface->insert_text        = ect_insert_text;
	iface->copy_text          = ect_copy_text;
	iface->cut_text           = ect_cut_text;
	iface->delete_text        = ect_delete_text;
	iface->paste_text         = ect_paste_text;
}

static void
ect_class_init (GalA11yECellTextClass *klass)
{
	AtkObjectClass *a11y      = ATK_OBJECT_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class              = g_type_class_ref (PARENT_TYPE);
	a11y->get_name            = ect_get_name;
	object_class->dispose     = ect_dispose;
}

static void
ect_action_init (GalA11yECellText *a11y)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (a11y);
	ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);
	if (ect->editable && e_table_model_is_cell_editable (gaec->cell_view->e_table_model, gaec->model_col, gaec->row))
		gal_a11y_e_cell_add_action (gaec,
				    _("edit"),
				    _("begin editing this cell"),
				    NULL,
				    (ACTION_FUNC) ect_do_action_edit);
}

/**
 * gal_a11y_e_cell_text_get_type:
 * @void: 
 * 
 * Registers the &GalA11yECellText class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GalA11yECellText class.
 **/
GType
gal_a11y_e_cell_text_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yECellTextClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ect_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yECellText),
			0,
			(GInstanceInitFunc) NULL,
			NULL /* value_cell_text */
		};

		static const GInterfaceInfo atk_text_info = {
			(GInterfaceInitFunc) ect_atk_text_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_editable_text_info = {
			(GInterfaceInitFunc) ect_atk_editable_text_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (PARENT_TYPE, "GalA11yECellText", &info, 0);
		g_type_add_interface_static (type, ATK_TYPE_TEXT, &atk_text_info);
		g_type_add_interface_static (type, ATK_TYPE_EDITABLE_TEXT, &atk_editable_text_info);
		gal_a11y_e_cell_type_add_action_interface (type);
	}

	return type;
}

static void
cell_text_destroyed (gpointer data)
{
	g_return_if_fail (GAL_A11Y_IS_E_CELL_TEXT (data));
	
	g_object_unref (data);
}

AtkObject *
gal_a11y_e_cell_text_new (ETableItem *item,
			  ECellView  *cell_view,
			  AtkObject  *parent,
			  int         model_col,
			  int         view_col,
			  int         row)
{
	AtkObject *a11y;
	GalA11yECell *gaec;
	GalA11yECellText *gaet;
	ECellText *ect; 

	a11y = g_object_new (gal_a11y_e_cell_text_get_type (), NULL);

	gal_a11y_e_cell_construct (a11y,
				   item,
				   cell_view,
				   parent,
				   model_col,
				   view_col,
				   row);
	gaet = GAL_A11Y_E_CELL_TEXT (a11y);

	/* will be unrefed in cell_text_destroyed */
	g_object_ref (a11y);

	gaet->inserted_id = g_signal_connect (E_CELL_TEXT (((ECellView *)cell_view)->ecell),		  
						"text_inserted", G_CALLBACK (ect_text_inserted_cb), a11y);
	gaet->deleted_id = g_signal_connect (E_CELL_TEXT (((ECellView *)cell_view)->ecell), 
					     "text_deleted", G_CALLBACK (ect_text_deleted_cb), a11y);

	g_object_weak_ref (G_OBJECT (((ECellView *)cell_view)->ecell),
			   (GWeakNotify) cell_text_destroyed,
			   a11y);

	ect_action_init (gaet);

	ect = E_CELL_TEXT (cell_view->ecell);
	gaec = GAL_A11Y_E_CELL (a11y);
	if (ect->editable && e_table_model_is_cell_editable (gaec->cell_view->e_table_model, gaec->model_col, gaec->row))
		gal_a11y_e_cell_add_state (gaec, ATK_STATE_EDITABLE, FALSE);
	else
		gal_a11y_e_cell_remove_state (gaec, ATK_STATE_EDITABLE, FALSE);

	return a11y;
}
