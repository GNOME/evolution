/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>
#include "gal-a11y-e-cell-text.h"
#include "gal-a11y-util.h"
#include <gal/e-table/e-cell-text.h>
#include <atk/atkobject.h>
#include <atk/atktext.h>
#include <atk/atkeditabletext.h>

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yECellTextClass))
static AtkObjectClass *parent_class;
#define PARENT_TYPE (gal_a11y_e_cell_get_type ())

/* XXX: these functions are undefined */
#define e_cell_text_get_selection(a,b,c,d,e) NULL
#define e_cell_text_set_selection(a,b,c,d,e) FALSE

/* Static functions */
static gchar *
ect_get_text (AtkText *text,
	      gint start_offset,
	      gint end_offset)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);
	char *ret_val;
	char *full_text =
		e_cell_text_get_text (ect, gaec->item->table_model, gaec->model_col, gaec->row);

	if (end_offset == -1)
		end_offset = strlen (full_text);
	else
		end_offset = g_utf8_offset_to_pointer (full_text, end_offset) - full_text;

	start_offset = g_utf8_offset_to_pointer (full_text, start_offset) - full_text;

	ret_val = g_strndup (full_text + start_offset, end_offset - start_offset);

	e_cell_text_free_text (ect, full_text);

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
	ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);
	gunichar ret_val;
	char *full_text;
	char *at_offset;

	full_text = e_cell_text_get_text (ect, gaec->item->table_model, gaec->model_col, gaec->row);
	at_offset = g_utf8_offset_to_pointer (full_text, offset);
	ret_val = g_utf8_get_char_validated (at_offset, -1);
	e_cell_text_free_text (ect, full_text);

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
	int start, end;
	if (e_cell_text_get_selection (gaec->cell_view,
				       gaec->view_col, gaec->row,
				       &start, &end)
	    && start == end) {
		char *full_text;
		int ret_val;
		ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);

		full_text = e_cell_text_get_text (ect, gaec->item->table_model, gaec->model_col, gaec->row);
		ret_val = g_utf8_pointer_to_offset (full_text, full_text + start);
		e_cell_text_free_text (ect, full_text);

		return ret_val;
	} else {
		return -1;
	}
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
	ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);
	int ret_val;

	char *full_text = e_cell_text_get_text (ect, gaec->item->table_model, gaec->model_col, gaec->row);

	ret_val = g_utf8_strlen (full_text, -1);
	e_cell_text_free_text (ect, full_text);
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
	int selection_start, selection_end;
	if (e_cell_text_get_selection (gaec->cell_view,
				       gaec->view_col, gaec->row,
				       &selection_start,
				       &selection_end) &&
	    selection_start != selection_end)
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
	ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);
	int selection_start, selection_end;
	if (selection_num == 0 &&
	    e_cell_text_get_selection (gaec->cell_view,
				       gaec->view_col, gaec->row,
				       &selection_start,
				       &selection_end) &&
	    selection_start != selection_end) {
		char *ret_val;
		char *full_text =
			e_cell_text_get_text (ect, gaec->item->table_model, gaec->model_col, gaec->row);

		ret_val = g_strndup (full_text + selection_start, selection_end - selection_start);

		if (start_offset)
			*start_offset = g_utf8_pointer_to_offset (full_text, full_text + selection_start);
		if (end_offset)
			*end_offset = g_utf8_pointer_to_offset (full_text, full_text + selection_end);

		e_cell_text_free_text (ect, full_text);

		return ret_val;
	}
	return NULL;
}


static gboolean
ect_add_selection (AtkText *text,
		   gint start_offset,
		   gint end_offset)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);
	int selection_start, selection_end;
	if (e_cell_text_get_selection (gaec->cell_view,
				       gaec->view_col, gaec->row,
				       &selection_start,
				       &selection_end) &&
	    selection_start == selection_end &&
	    start_offset != end_offset) {
		char *full_text;

		full_text = e_cell_text_get_text (ect, gaec->item->table_model, gaec->model_col, gaec->row);
		start_offset = g_utf8_offset_to_pointer (full_text, start_offset) - full_text;
		end_offset = g_utf8_offset_to_pointer (full_text, end_offset) - full_text;
		e_cell_text_free_text (ect, full_text);

		return e_cell_text_set_selection (gaec->cell_view,
						  gaec->view_col, gaec->row,
						  start_offset, end_offset);
	}
	return FALSE;
}


static gboolean
ect_remove_selection (AtkText *text,
		      gint selection_num)
{
	/* Unimplemented */
	return FALSE;
}


static gboolean
ect_set_selection (AtkText *text,
		   gint selection_num,
		   gint start_offset,
		   gint end_offset)
{
	/* Unimplemented */
	return FALSE;
}


static gboolean
ect_set_caret_offset (AtkText *text,
		      gint offset)
{
	GalA11yECell *gaec = GAL_A11Y_E_CELL (text);
	ECellText *ect = E_CELL_TEXT (gaec->cell_view->ecell);
	char *full_text;

	full_text = e_cell_text_get_text (ect, gaec->item->table_model, gaec->model_col, gaec->row);
	offset = g_utf8_offset_to_pointer (full_text, offset) - full_text;
	e_cell_text_free_text (ect, full_text);

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

	char *full_text = e_cell_text_get_text (ect, gaec->item->table_model, gaec->model_col, gaec->row);
	char *result = g_strdup_printf ("%.*s%.*s%s", *position, full_text, length, string, full_text + *position);

	e_cell_text_set_value (ect, gaec->item->table_model, gaec->model_col, gaec->row, result);

	*position += length;

	g_free (result);
	e_cell_text_free_text (ect, full_text);
}

static void
ect_copy_text (AtkEditableText *text,
	       gint start_pos,
	       gint end_pos)
{
	/* Unimplemented */
}

static void
ect_cut_text (AtkEditableText *text,
	      gint start_pos,
	      gint end_pos)
{
	/* Unimplemented */
}

static void
ect_delete_text (AtkEditableText *text,
		 gint start_pos,
		 gint end_pos)
{
	/* Unimplemented */
}

static void
ect_paste_text (AtkEditableText *text,
		gint position)
{
	/* Unimplemented */
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
	parent_class                          = g_type_class_ref (PARENT_TYPE);
}

static void
ect_init (GalA11yECellText *a11y)
{
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
			(GInstanceInitFunc) ect_init,
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
	}

	return type;
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

	a11y = g_object_new (gal_a11y_e_cell_text_get_type (), NULL);

	gal_a11y_e_cell_construct (a11y,
				   item,
				   cell_view,
				   parent,
				   model_col,
				   view_col,
				   row);
	return a11y;
}
