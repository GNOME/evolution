/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>
#include "gal-a11y-e-text.h"
#include "gal-a11y-util.h"
#include <atk/atkobject.h>
#include <atk/atktable.h>
#include <atk/atkcomponent.h>
#include <atk/atkobjectfactory.h>
#include <atk/atkregistry.h>
#include <atk/atkgobjectaccessible.h>
#include "gal/e-text/e-text.h"
#include <gtk/gtkmain.h>

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yETextClass))
static GObjectClass *parent_class;
static AtkComponentIface *component_parent_iface;
static GType parent_type;
static gint priv_offset;
static GQuark		quark_accessible_object = 0;
#define GET_PRIVATE(object) ((GalA11yETextPrivate *) (((char *) object) + priv_offset))
#define PARENT_TYPE (parent_type)

struct _GalA11yETextPrivate {
	int dummy;
};

static void
et_dispose (GObject *object)
{
	if (parent_class->dispose)
		parent_class->dispose (object);
}

/* Static functions */

static void
et_get_extents (AtkComponent *component,
		gint *x,
		gint *y,
		gint *width,
		gint *height,
		AtkCoordType coord_type)
{
	EText *item = E_TEXT (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (component)));
	double real_width;
	double real_height;
	int fake_width;
	int fake_height;

	if (component_parent_iface &&
	    component_parent_iface->get_extents)
		component_parent_iface->get_extents (component,
						     x,
						     y,
						     &fake_width,
						     &fake_height,
						     coord_type);

	gtk_object_get (GTK_OBJECT (item),
			"text_width", &real_width,
			"text_height", &real_height,
			NULL);

	if (width)
		*width = real_width;
	if (height) 
		*height = real_height;
}

static const gchar *
et_get_full_text (AtkText *text)
{
	EText *etext = E_TEXT (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));
	ETextModel *model;
	const char *full_text;

	gtk_object_get (GTK_OBJECT (etext),
			"model", &model,
			NULL);

	full_text = e_text_model_get_text (model);

	return full_text;
}

static void
et_set_full_text (AtkEditableText *text,
		  const char *full_text)
{
	EText *etext = E_TEXT (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));
	ETextModel *model;

	gtk_object_get (GTK_OBJECT (etext),
			"model", &model,
			NULL);

	e_text_model_set_text (model, full_text);
}

static gchar *
et_get_text (AtkText *text,
	     gint start_offset,
	     gint end_offset)
{
	const char *full_text = et_get_full_text (text);

	if (end_offset == -1)
		end_offset = strlen (full_text);
	else
		end_offset = g_utf8_offset_to_pointer (full_text, end_offset) - full_text;

	start_offset = g_utf8_offset_to_pointer (full_text, start_offset) - full_text;

	return g_strndup (full_text + start_offset, end_offset - start_offset);
}

static gchar *
et_get_text_after_offset (AtkText *text,
			  gint offset,
			  AtkTextBoundary boundary_type,
			  gint *start_offset,
			  gint *end_offset)
{
	/* Unimplemented */
	return NULL;
}

static gchar *
et_get_text_at_offset (AtkText *text,
		       gint offset,
		       AtkTextBoundary boundary_type,
		       gint *start_offset,
		       gint *end_offset)
{
	/* Unimplemented */
	return NULL;
}

static gunichar
et_get_character_at_offset (AtkText *text,
			    gint offset)
{
	const char *full_text = et_get_full_text (text);
	char *at_offset;

	at_offset = g_utf8_offset_to_pointer (full_text, offset);
	return g_utf8_get_char_validated (at_offset, -1);
}


static gchar*
et_get_text_before_offset (AtkText *text,
			   gint offset,
			   AtkTextBoundary boundary_type,
			   gint *start_offset,
			   gint *end_offset)
{
	/* Unimplemented */
	return NULL;
}


static gint
et_get_caret_offset (AtkText *text)
{
	EText *etext = E_TEXT (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));
	const char *full_text = et_get_full_text (text);
	int offset;

	gtk_object_get (GTK_OBJECT (etext),
			"cursor_pos", &offset,
			NULL);
	offset = g_utf8_pointer_to_offset (full_text, full_text + offset);
	return offset;
}


static AtkAttributeSet*
et_get_run_attributes (AtkText *text,
		       gint offset,
		       gint *start_offset,
		       gint *end_offset)
{
	/* Unimplemented */
	return NULL;
}


static AtkAttributeSet*
et_get_default_attributes (AtkText *text)
{
	/* Unimplemented */
	return NULL;
}


static void
et_get_character_extents (AtkText *text,
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
et_get_character_count (AtkText *text)
{
	const char *full_text = et_get_full_text (text);

	return g_utf8_strlen (full_text, -1);
}


static gint
et_get_offset_at_point (AtkText *text,
			gint x,
			gint y,
			AtkCoordType coords)
{
	/* Unimplemented */
	return 0;
}


static gint 
et_get_n_selections (AtkText *text)
{
	EText *etext = E_TEXT (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));
	if (etext->selection_start !=
	    etext->selection_end)
		return 1;
	return 0;
}


static gchar*
et_get_selection (AtkText *text,
		  gint selection_num,
		  gint *start_offset,
		  gint *end_offset)
{
	EText *etext = E_TEXT (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));
	if (selection_num == 0 &&
	    etext->selection_start != etext->selection_end) {
		const char *full_text = et_get_full_text (text);

		if (start_offset)
			*start_offset = g_utf8_pointer_to_offset (full_text, full_text + etext->selection_start);
		if (end_offset)
			*end_offset = g_utf8_pointer_to_offset (full_text, full_text + etext->selection_end);

		return g_strndup (full_text + etext->selection_start, etext->selection_end - etext->selection_start);
	}
	return NULL;
}


static gboolean
et_add_selection (AtkText *text,
		  gint start_offset,
		  gint end_offset)
{
	EText *etext = E_TEXT (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));
	if (etext->selection_start == etext->selection_end &&
	    start_offset != end_offset) {
		ETextEventProcessorCommand command;
		const char *full_text = et_get_full_text (text);
		ETextEventProcessor *tep;

		start_offset = g_utf8_offset_to_pointer (full_text, start_offset) - full_text;
		end_offset = g_utf8_offset_to_pointer (full_text, end_offset) - full_text;

		gtk_object_get (GTK_OBJECT (etext),
				"tep", &tep,
				NULL);

		command.time = gtk_get_current_event_time ();

		command.action = E_TEP_MOVE;
		command.position = E_TEP_VALUE;
		command.value = start_offset;
		g_signal_emit_by_name (tep, "command", 0, &command);

		command.action = E_TEP_SELECT;
		command.value = end_offset;
		g_signal_emit_by_name (tep, "command", 0, &command);
		return TRUE;
	}
	return FALSE;
}


static gboolean
et_remove_selection (AtkText *text,
		     gint selection_num)
{
	/* Unimplemented */
	return FALSE;
}


static gboolean
et_set_selection (AtkText *text,
		  gint selection_num,
		  gint start_offset,
		  gint end_offset)
{
	/* Unimplemented */
	return FALSE;
}


static gboolean
et_set_caret_offset (AtkText *text,
		     gint offset)
{
	EText *etext = E_TEXT (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));
	const char *full_text = et_get_full_text (text);

	offset = g_utf8_offset_to_pointer (full_text, offset) - full_text;
	gtk_object_set (GTK_OBJECT (etext),
			"cursor_pos", &offset,
			NULL);
	return TRUE;
}

static gboolean
et_set_run_attributes (AtkEditableText *text,
		       AtkAttributeSet *attrib_set,
		       gint start_offset,
		       gint end_offset)
{
	/* Unimplemented */
	return FALSE;
}

static void
et_set_text_contents (AtkEditableText *text,
		      const gchar *string)
{
	et_set_full_text (text, string);
}

static void
et_insert_text (AtkEditableText *text,
		const gchar *string,
		gint length,
		gint *position)
{
	/* Utf8 unimplemented */
	char *result;

	const char *full_text = et_get_full_text (ATK_TEXT (text));
	if (full_text == NULL)
		return;

	result = g_strdup_printf ("%.*s%.*s%s", *position, full_text, length, string, full_text + *position);

	et_set_full_text (text, result);

	*position += length;

	g_free (result);
}

static void
et_copy_text (AtkEditableText *text,
	      gint start_pos,
	      gint end_pos)
{
	GObject *obj;
	EText *etext;

	g_return_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text));
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return;

	g_return_if_fail (E_IS_TEXT (obj));
	etext = E_TEXT (obj);

	if (start_pos != end_pos) {
		etext->selection_start = start_pos;
		etext->selection_end = end_pos;
		e_text_copy_clipboard (etext);
	}
}

static void
et_delete_text (AtkEditableText *text,
		gint start_pos,
		gint end_pos)
{
	GObject *obj;
	EText *etext;

	g_return_if_fail (ATK_IS_GOBJECT_ACCESSIBLE(text));
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return;

	g_return_if_fail (E_IS_TEXT (obj));
	etext = E_TEXT (obj);

	etext->selection_start = start_pos;
	etext->selection_end = end_pos;

	e_text_delete_selection (etext);
}

static void
et_cut_text (AtkEditableText *text,
	     gint start_pos,
	     gint end_pos)
{
	et_copy_text (text, start_pos, end_pos);
	et_delete_text (text, start_pos, end_pos);
}

static void
et_paste_text (AtkEditableText *text,
	       gint position)
{
	GObject *obj;
	EText *etext;

	g_return_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text));
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return;

	g_return_if_fail (E_IS_TEXT (obj));
	etext = E_TEXT (obj);

	gtk_object_set (GTK_OBJECT (etext),
			"cursor_pos", position,
			NULL);
	e_text_paste_clipboard (etext);
}

static void
et_atk_component_iface_init (AtkComponentIface *iface)
{
	iface->get_extents = et_get_extents;
}

static void
et_atk_text_iface_init (AtkTextIface *iface)
{
	iface->get_text                = et_get_text;
	iface->get_text_after_offset   = et_get_text_after_offset;
	iface->get_text_at_offset      = et_get_text_at_offset;
	iface->get_character_at_offset = et_get_character_at_offset;
	iface->get_text_before_offset  = et_get_text_before_offset;
	iface->get_caret_offset        = et_get_caret_offset;
	iface->get_run_attributes      = et_get_run_attributes;
	iface->get_default_attributes  = et_get_default_attributes;
	iface->get_character_extents   = et_get_character_extents;
	iface->get_character_count     = et_get_character_count;
	iface->get_offset_at_point     = et_get_offset_at_point;
	iface->get_n_selections        = et_get_n_selections;
	iface->get_selection           = et_get_selection;
	iface->add_selection           = et_add_selection;
	iface->remove_selection        = et_remove_selection;
	iface->set_selection           = et_set_selection;
	iface->set_caret_offset        = et_set_caret_offset;
}

static void
et_atk_editable_text_iface_init (AtkEditableTextIface *iface)
{
	iface->set_run_attributes = et_set_run_attributes;
	iface->set_text_contents  = et_set_text_contents;
	iface->insert_text        = et_insert_text;
	iface->copy_text          = et_copy_text;
	iface->cut_text           = et_cut_text;
	iface->delete_text        = et_delete_text;
	iface->paste_text         = et_paste_text;
}

static void
et_class_init (GalA11yETextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	quark_accessible_object               = g_quark_from_static_string ("gtk-accessible-object");

	parent_class                          = g_type_class_ref (PARENT_TYPE);

	component_parent_iface                = g_type_interface_peek(parent_class, ATK_TYPE_COMPONENT);
	
	object_class->dispose                 = et_dispose;
}

static void
et_init (GalA11yEText *a11y)
{
#if 0
	GalA11yETextPrivate *priv;

	priv = GET_PRIVATE (a11y);
#endif
}

/**
 * gal_a11y_e_text_get_type:
 * @void: 
 * 
 * Registers the &GalA11yEText class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GalA11yEText class.
 **/
GType
gal_a11y_e_text_get_type (void)
{
	static GType type = 0;

	if (!type) {
		AtkObjectFactory *factory;

		GTypeInfo info = {
			sizeof (GalA11yETextClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) et_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yEText),
			0,
			(GInstanceInitFunc) et_init,
			NULL /* value_text */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) et_atk_component_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};
		static const GInterfaceInfo atk_text_info = {
			(GInterfaceInitFunc) et_atk_text_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};
		static const GInterfaceInfo atk_editable_text_info = {
			(GInterfaceInitFunc) et_atk_editable_text_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		factory = atk_registry_get_factory (atk_get_default_registry (), GNOME_TYPE_CANVAS_ITEM);
		parent_type = atk_object_factory_get_accessible_type (factory);

		type = gal_a11y_type_register_static_with_private (PARENT_TYPE, "GalA11yEText", &info, 0,
								   sizeof (GalA11yETextPrivate), &priv_offset);

		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
		g_type_add_interface_static (type, ATK_TYPE_TEXT, &atk_text_info);
		g_type_add_interface_static (type, ATK_TYPE_EDITABLE_TEXT, &atk_editable_text_info);
	}

	return type;
}
