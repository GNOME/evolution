/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-text.h"

#include <string.h>

#include <gtk/gtk.h>

#include "e-text.h"
#include "e-text-model-repos.h"
#include "gal-a11y-e-text-factory.h"
#include "gal-a11y-util.h"

static GObjectClass *parent_class;
static AtkComponentIface *component_parent_iface;
static GType parent_type;
static gint priv_offset;
static GQuark		quark_accessible_object = 0;
#define PARENT_TYPE (parent_type)

struct _GalA11yETextPrivate {
	gint dummy;
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
	EText *item = E_TEXT (atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (component)));
	gdouble real_width;
	gdouble real_height;
	gint fake_width;
	gint fake_height;

	if (component_parent_iface &&
	    component_parent_iface->get_extents)
		component_parent_iface->get_extents (component,
						     x,
						     y,
						     &fake_width,
						     &fake_height,
						     coord_type);

	g_object_get (
		item,
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
	GObject *obj;
	EText *etext;
	ETextModel *model;
	const gchar *full_text;

	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return "";

	etext = E_TEXT (obj);
	g_object_get (etext, "model", &model, NULL);

	full_text = e_text_model_get_text (model);

	return full_text;
}

static void
et_set_full_text (AtkEditableText *text,
                  const gchar *full_text)
{
	GObject *obj;
	EText *etext;
	ETextModel *model;

	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return;

	etext = E_TEXT (obj);
	g_object_get (etext, "model", &model, NULL);

	e_text_model_set_text (model, full_text);
}

static gchar *
et_get_text (AtkText *text,
             gint start_offset,
             gint end_offset)
{
	gint start, end, real_start, real_end, len;
	const gchar *full_text = et_get_full_text (text);
	if (full_text == NULL)
		return NULL;
	len = g_utf8_strlen (full_text, -1);

	start = MIN (MAX (0, start_offset), len);
	end = MIN (MAX (-1, end_offset), len);

	if (end_offset == -1)
		end = strlen (full_text);
	else
		end = g_utf8_offset_to_pointer (full_text, end) - full_text;

	start = g_utf8_offset_to_pointer (full_text, start) - full_text;

	real_start = MIN (start, end);
	real_end = MAX (start, end);

	return g_strndup (full_text + real_start, real_end - real_start);
}

static gboolean
is_a_seperator (gunichar c)
{
	return g_unichar_ispunct (c) || g_unichar_isspace (c);
}

static gint
find_word_start (const gchar *text,
                 gint begin_offset,
                 gint step)
{
	gint offset;
	gchar *at_offset;
	gunichar current, previous;
	gint len;

	offset = begin_offset;
	len = g_utf8_strlen (text, -1);

	while (offset > 0 && offset < len) {
		at_offset = g_utf8_offset_to_pointer (text, offset);
		current = g_utf8_get_char_validated (at_offset, -1);
		at_offset = g_utf8_offset_to_pointer (text, offset - 1);
		previous = g_utf8_get_char_validated (at_offset, -1);
		if ((!is_a_seperator (current)) && is_a_seperator (previous))
			break;
		offset += step;
	}

	return offset;
}

static gint
find_word_end (const gchar *text,
               gint begin_offset,
               gint step)
{
	gint offset;
	gchar *at_offset;
	gunichar current, previous;
	gint len;

	offset = begin_offset;
	len = g_utf8_strlen (text, -1);

	while (offset > 0 && offset < len) {
		at_offset = g_utf8_offset_to_pointer (text, offset);
		current = g_utf8_get_char_validated (at_offset, -1);
		at_offset = g_utf8_offset_to_pointer (text, offset - 1);
		previous = g_utf8_get_char_validated (at_offset, -1);
		if (is_a_seperator (current) && (!is_a_seperator (previous)))
			break;
		offset += step;
	}

	return offset;
}

static gint
find_sentence_start (const gchar *text,
                     gint begin_offset,
                     gint step)
{
	gint offset, last_word_end, len;
	gchar *at_offset;
	gunichar ch;
	gint i;

	offset = find_word_start (text, begin_offset, step);
	len = g_utf8_strlen (text, -1);

	while (offset > 0 && offset <len) {
		last_word_end = find_word_end (text, offset - 1, -1);
		if (last_word_end == 0)
			break;
		for (i = last_word_end; i < offset; i++) {
			at_offset = g_utf8_offset_to_pointer (text, i);
			ch = g_utf8_get_char_validated (at_offset, -1);
			if (ch == '.' || ch == '!' || ch == '?')
				return offset;
		}

		offset = find_word_start (text, offset + step, step);
	}

	return offset;
}

static gint
find_sentence_end (const gchar *text,
                   gint begin_offset,
                   gint step)
{
	gint offset;
	gchar *at_offset;
	gunichar previous;
	gint len;

	offset = begin_offset;
	len = g_utf8_strlen (text, -1);

	while (offset > 0 && offset < len) {
		at_offset = g_utf8_offset_to_pointer (text, offset - 1);
		previous = g_utf8_get_char_validated (at_offset, -1);
		if (previous == '.' || previous == '!' || previous == '?')
			break;
		offset += step;
	}

	return offset;
}

static gint
find_line_start (const gchar *text,
                     gint begin_offset,
                     gint step)
{
	gint offset;
	gchar *at_offset;
	gunichar previous;
	gint len;

	offset = begin_offset;
	len = g_utf8_strlen (text, -1);

	while (offset > 0 && offset < len) {
		at_offset = g_utf8_offset_to_pointer (text, offset - 1);
		previous = g_utf8_get_char_validated (at_offset, -1);
		if (previous == '\n' || previous == '\r')
			break;
		offset += step;
	}

	return offset;
}

static gint
find_line_end (const gchar *text,
                     gint begin_offset,
                     gint step)
{
	gint offset;
	gchar *at_offset;
	gunichar current;
	gint len;

	offset = begin_offset;
	len = g_utf8_strlen (text, -1);

	while (offset >= 0 && offset < len) {
		at_offset = g_utf8_offset_to_pointer (text, offset);
		current = g_utf8_get_char_validated (at_offset, -1);
		if (current == '\n' || current == '\r')
			break;
		offset += step;
	}

	return offset;
}

static gchar *
et_get_text_after_offset (AtkText *text,
                          gint offset,
                          AtkTextBoundary boundary_type,
                          gint *start_offset,
                          gint *end_offset)
{
	gint start, end, len;
	const gchar *full_text = et_get_full_text (text);
	g_return_val_if_fail (full_text, NULL);

	switch (boundary_type)
	{
	case ATK_TEXT_BOUNDARY_CHAR:
		start = offset + 1;
		end = offset + 2;
		break;
	case ATK_TEXT_BOUNDARY_WORD_START:
		start = find_word_start (full_text, offset + 1, 1);
		end = find_word_start (full_text, start + 1, 1);
		break;
	case ATK_TEXT_BOUNDARY_WORD_END:
		start = find_word_end (full_text, offset + 1, 1);
		end = find_word_end (full_text, start + 1, 1);
		break;
	case ATK_TEXT_BOUNDARY_SENTENCE_START:
		start = find_sentence_start (full_text, offset + 1, 1);
		end = find_sentence_start (full_text, start + 1, 1);
		break;
	case ATK_TEXT_BOUNDARY_SENTENCE_END:
		start = find_sentence_end (full_text, offset + 1, 1);
		end = find_sentence_end (full_text, start + 1, 1);
		break;
	case ATK_TEXT_BOUNDARY_LINE_START:
		start = find_line_start (full_text, offset + 1, 1);
		end = find_line_start (full_text, start + 1, 1);
		break;
	case ATK_TEXT_BOUNDARY_LINE_END:
		start = find_line_end (full_text, offset + 1, 1);
		end = find_line_end (full_text, start + 1, 1);
		break;
	default:
		return NULL;
	}

	len = g_utf8_strlen (full_text, -1);
	if (start_offset)
		*start_offset = MIN (MAX (0, start), len);
	if (end_offset)
		*end_offset = MIN (MAX (0, end), len);
	return et_get_text (text, start, end);
}

static gchar *
et_get_text_at_offset (AtkText *text,
                       gint offset,
                       AtkTextBoundary boundary_type,
                       gint *start_offset,
                       gint *end_offset)
{
	gint start, end, len;
	const gchar *full_text = et_get_full_text (text);
	g_return_val_if_fail (full_text, NULL);

	switch (boundary_type)
	{
	case ATK_TEXT_BOUNDARY_CHAR:
		start = offset;
		end = offset + 1;
		break;
	case ATK_TEXT_BOUNDARY_WORD_START:
		start = find_word_start (full_text, offset - 1, -1);
		end = find_word_start (full_text, offset, 1);
		break;
	case ATK_TEXT_BOUNDARY_WORD_END:
		start = find_word_end (full_text, offset, -1);
		end = find_word_end (full_text, offset + 1, 1);
		break;
	case ATK_TEXT_BOUNDARY_SENTENCE_START:
		start = find_sentence_start (full_text, offset - 1, -1);
		end = find_sentence_start (full_text, offset, 1);
		break;
	case ATK_TEXT_BOUNDARY_SENTENCE_END:
		start = find_sentence_end (full_text, offset, -1);
		end = find_sentence_end (full_text, offset + 1, 1);
		break;
	case ATK_TEXT_BOUNDARY_LINE_START:
		start = find_line_start (full_text, offset - 1, -1);
		end = find_line_start (full_text, offset, 1);
		break;
	case ATK_TEXT_BOUNDARY_LINE_END:
		start = find_line_end (full_text, offset, -1);
		end = find_line_end (full_text, offset + 1, 1);
		break;
	default:
		return NULL;
	}

	len = g_utf8_strlen (full_text, -1);
	if (start_offset)
		*start_offset = MIN (MAX (0, start), len);
	if (end_offset)
		*end_offset = MIN (MAX (0, end), len);
	return et_get_text (text, start, end);
}

static gunichar
et_get_character_at_offset (AtkText *text,
                            gint offset)
{
	const gchar *full_text = et_get_full_text (text);
	gchar *at_offset;

	at_offset = g_utf8_offset_to_pointer (full_text, offset);
	return g_utf8_get_char_validated (at_offset, -1);
}

static gchar *
et_get_text_before_offset (AtkText *text,
                           gint offset,
                           AtkTextBoundary boundary_type,
                           gint *start_offset,
                           gint *end_offset)
{
	gint start, end, len;
	const gchar *full_text = et_get_full_text (text);
	g_return_val_if_fail (full_text, NULL);

	switch (boundary_type)
	{
	case ATK_TEXT_BOUNDARY_CHAR:
		start = offset - 1;
		end = offset;
		break;
	case ATK_TEXT_BOUNDARY_WORD_START:
		end = find_word_start (full_text, offset - 1, -1);
		start = find_word_start (full_text, end - 1, -1);
		break;
	case ATK_TEXT_BOUNDARY_WORD_END:
		end = find_word_end (full_text, offset, -1);
		start = find_word_end (full_text, end - 1, -1);
		break;
	case ATK_TEXT_BOUNDARY_SENTENCE_START:
		end = find_sentence_start (full_text, offset, -1);
		start = find_sentence_start (full_text, end - 1, -1);
		break;
	case ATK_TEXT_BOUNDARY_SENTENCE_END:
		end = find_sentence_end (full_text, offset, -1);
		start = find_sentence_end (full_text, end - 1, -1);
		break;
	case ATK_TEXT_BOUNDARY_LINE_START:
		end = find_line_start (full_text, offset, -1);
		start = find_line_start (full_text, end - 1, -1);
		break;
	case ATK_TEXT_BOUNDARY_LINE_END:
		end = find_line_end (full_text, offset, -1);
		start = find_line_end (full_text, end - 1, -1);
		break;
	default:
		return NULL;
	}

	len = g_utf8_strlen (full_text, -1);
	if (start_offset)
		*start_offset = MIN (MAX (0, start), len);
	if (end_offset)
		*end_offset = MIN (MAX (0, end), len);
	return et_get_text (text, start, end);
}

static gint
et_get_caret_offset (AtkText *text)
{
	GObject *obj;
	EText *etext;
	gint offset;

	g_return_val_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text), -1);
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return -1;

	g_return_val_if_fail (E_IS_TEXT (obj), -1);
	etext = E_TEXT (obj);

	g_object_get (etext, "cursor_pos", &offset, NULL);
	return offset;
}

static AtkAttributeSet *
et_get_run_attributes (AtkText *text,
                       gint offset,
                       gint *start_offset,
                       gint *end_offset)
{
	/* Unimplemented */
	return NULL;
}

static AtkAttributeSet *
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
	GObject *obj;
	EText *etext;
	GnomeCanvas *canvas;
	gint x_widget, y_widget, x_window, y_window;
	GdkWindow *window;
	GtkWidget *widget;
	PangoRectangle pango_pos;

	g_return_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text));
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
	    return;
	g_return_if_fail (E_IS_TEXT (obj));
	etext = E_TEXT (obj);
	canvas = GNOME_CANVAS_ITEM (etext)->canvas;
	widget = GTK_WIDGET (canvas);
	window = gtk_widget_get_window (widget);
	gdk_window_get_origin (window, &x_widget, &y_widget);

	pango_layout_index_to_pos (etext->layout, offset, &pango_pos);
	pango_pos.x = PANGO_PIXELS (pango_pos.x);
	pango_pos.y = PANGO_PIXELS (pango_pos.y);
	pango_pos.width = (pango_pos.width + PANGO_SCALE / 2) / PANGO_SCALE;
	pango_pos.height = (pango_pos.height + PANGO_SCALE / 2) / PANGO_SCALE;

        *x = pango_pos.x + x_widget;
        *y = pango_pos.y + y_widget;

	*width  = pango_pos.width;
	*height = pango_pos.height;

        *x += etext->xofs;
        *y += etext->yofs;

	if (etext->editing) {
                *x -= etext->xofs_edit;
                *y -= etext->yofs_edit;
	}

        *x += etext->cx;
        *y += etext->cy;

	if (coords == ATK_XY_WINDOW) {
		window = gdk_window_get_toplevel (window);
		gdk_window_get_origin (window, &x_window, &y_window);
		*x -= x_window;
		*y -= y_window;
	}
	else if (coords == ATK_XY_SCREEN) {
	}
	else {
		*x = 0;
		*y = 0;
		*height = 0;
		*width = 0;
	}
}

static gint
et_get_character_count (AtkText *text)
{
	const gchar *full_text = et_get_full_text (text);

	return g_utf8_strlen (full_text, -1);
}

static gint
et_get_offset_at_point (AtkText *text,
                        gint x,
                        gint y,
                        AtkCoordType coords)
{
	GObject *obj;
	EText *etext;
	GnomeCanvas *canvas;
	gint x_widget, y_widget, x_window, y_window;
	GdkWindow *window;
	GtkWidget *widget;
	gint index;
	gint trailing;

	g_return_val_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text), -1);
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
	    return -1;
	g_return_val_if_fail (E_IS_TEXT (obj), -1);
	etext = E_TEXT (obj);
	canvas = GNOME_CANVAS_ITEM (etext)->canvas;
	widget = GTK_WIDGET (canvas);
	window = gtk_widget_get_window (widget);
	gdk_window_get_origin (window, &x_widget, &y_widget);

	if (coords == ATK_XY_SCREEN) {
		x = x - x_widget;
		y = y - y_widget;
	}
	else if (coords == ATK_XY_WINDOW) {
		window = gdk_window_get_toplevel (window);
		gdk_window_get_origin (window, &x_window, &y_window);
		x = x - x_widget + x_window;
		y = y - y_widget + y_window;
	}
	else
		return -1;

	x -= etext->xofs;
	y -= etext->yofs;

	if (etext->editing) {
		x += etext->xofs_edit;
		y += etext->yofs_edit;
	}

	x -= etext->cx;
	y -= etext->cy;

	pango_layout_xy_to_index (
		etext->layout,
		x * PANGO_SCALE - PANGO_SCALE / 2,
		y * PANGO_SCALE - PANGO_SCALE / 2,
		&index,
		&trailing);

	return g_utf8_pointer_to_offset (etext->text, etext->text + index + trailing);
}

static gint
et_get_n_selections (AtkText *text)
{
	EText *etext;
	GObject *obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));

	if (obj == NULL)
		return -1;
	etext = E_TEXT (obj);

	if (etext->selection_start !=
	    etext->selection_end)
		return 1;
	return 0;
}

static gchar *
et_get_selection (AtkText *text,
                  gint selection_num,
                  gint *start_offset,
                  gint *end_offset)
{
	gint start, end, real_start, real_end, len;
	EText *etext;
	if (selection_num == 0) {
		const gchar *full_text = et_get_full_text (text);
		if (full_text == NULL)
			return NULL;
		len = g_utf8_strlen (full_text, -1);
		etext = E_TEXT (atk_gobject_accessible_get_object (
			ATK_GOBJECT_ACCESSIBLE (text)));
		start = MIN (etext->selection_start, etext->selection_end);
		end = MAX (etext->selection_start, etext->selection_end);
		start = MIN (MAX (0, start), len);
		end = MIN (MAX (0, end), len);
		if (start != end) {
			if (start_offset)
				*start_offset = start;
			if (end_offset)
				*end_offset = end;
			real_start = g_utf8_offset_to_pointer (full_text, start) - full_text;
			real_end = g_utf8_offset_to_pointer (full_text, end) - full_text;
			return g_strndup (full_text + real_start, real_end - real_start);
		}
	}

	return NULL;
}

static gboolean
et_add_selection (AtkText *text,
                  gint start_offset,
                  gint end_offset)
{
	GObject *obj;
	EText *etext;

	g_return_val_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text), FALSE);
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return FALSE;
	g_return_val_if_fail (E_IS_TEXT (obj), FALSE);
	etext = E_TEXT (obj);

	g_return_val_if_fail (start_offset >= 0, FALSE);
	g_return_val_if_fail (start_offset >= -1, FALSE);
	if (end_offset == -1)
		end_offset = et_get_character_count (text);

	if (start_offset != end_offset) {
		gint real_start, real_end;
		real_start = MIN (start_offset, end_offset);
		real_end = MAX (start_offset, end_offset);
		etext->selection_start = real_start;
		etext->selection_end = real_end;

		gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (etext));
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (etext));

		g_signal_emit_by_name (ATK_OBJECT (text), "text_selection_changed");

		return TRUE;
	}

	return FALSE;
}

static gboolean
et_remove_selection (AtkText *text,
                     gint selection_num)
{
	GObject *obj;
	EText *etext;

	g_return_val_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text), FALSE);
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return FALSE;
	g_return_val_if_fail (E_IS_TEXT (obj), FALSE);
	etext = E_TEXT (obj);

	if (selection_num == 0
	    && etext->selection_start != etext->selection_end) {
		etext->selection_end = etext->selection_start;
		g_signal_emit_by_name (ATK_OBJECT (text), "text_selection_changed");
		return TRUE;
	}

	return FALSE;
}

static gboolean
et_set_selection (AtkText *text,
                  gint selection_num,
                  gint start_offset,
                  gint end_offset)
{
	GObject *obj;

	g_return_val_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text), FALSE);
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return FALSE;
	g_return_val_if_fail (E_IS_TEXT (obj), FALSE);
	if (selection_num == 0)
		return et_add_selection (text, start_offset, end_offset);
	return FALSE;
}

static gboolean
et_set_caret_offset (AtkText *text,
                     gint offset)
{
	GObject *obj;
	EText *etext;

	g_return_val_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text), FALSE);
	obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text));
	if (obj == NULL)
		return FALSE;

	g_return_val_if_fail (E_IS_TEXT (obj), FALSE);
	etext = E_TEXT (obj);

	if (offset < -1)
		return FALSE;
	else {
		ETextEventProcessorCommand command;

		if (offset == -1)
			offset = et_get_character_count (text);

		command.action = E_TEP_MOVE;
		command.position = E_TEP_VALUE;
		command.value = offset;
		command.time = GDK_CURRENT_TIME;
		g_signal_emit_by_name (etext->tep, "command", &command);
		return TRUE;
	}
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
	gchar *result;

	const gchar *full_text = et_get_full_text (ATK_TEXT (text));
	if (full_text == NULL)
		return;

	result = g_strdup_printf (
		"%.*s%.*s%s", *position, full_text,
		length, string, full_text + *position);

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

	g_return_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (text));
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

	g_object_set (etext, "cursor_pos", position, NULL);
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
	iface->get_text = et_get_text;
	iface->get_text_after_offset = et_get_text_after_offset;
	iface->get_text_at_offset = et_get_text_at_offset;
	iface->get_character_at_offset = et_get_character_at_offset;
	iface->get_text_before_offset = et_get_text_before_offset;
	iface->get_caret_offset = et_get_caret_offset;
	iface->get_run_attributes = et_get_run_attributes;
	iface->get_default_attributes = et_get_default_attributes;
	iface->get_character_extents = et_get_character_extents;
	iface->get_character_count = et_get_character_count;
	iface->get_offset_at_point = et_get_offset_at_point;
	iface->get_n_selections = et_get_n_selections;
	iface->get_selection = et_get_selection;
	iface->add_selection = et_add_selection;
	iface->remove_selection = et_remove_selection;
	iface->set_selection = et_set_selection;
	iface->set_caret_offset = et_set_caret_offset;
}

static void
et_atk_editable_text_iface_init (AtkEditableTextIface *iface)
{
	iface->set_run_attributes = et_set_run_attributes;
	iface->set_text_contents = et_set_text_contents;
	iface->insert_text = et_insert_text;
	iface->copy_text = et_copy_text;
	iface->cut_text = et_cut_text;
	iface->delete_text = et_delete_text;
	iface->paste_text = et_paste_text;
}

static void
_et_reposition_cb (ETextModel *model,
                   ETextModelReposFn fn,
                   gpointer repos_data,
                   gpointer user_data)
{
	AtkObject *accessible;
	AtkText *text;

	accessible = ATK_OBJECT (user_data);
	text = ATK_TEXT (accessible);

	if (fn == e_repos_delete_shift) {
		EReposDeleteShift *info = (EReposDeleteShift *) repos_data;
		g_signal_emit_by_name (text, "text-changed::delete", info->pos, info->len);
	}
	else if (fn == e_repos_insert_shift) {
		EReposInsertShift *info = (EReposInsertShift *) repos_data;
		g_signal_emit_by_name (text, "text-changed::insert", info->pos, info->len);
	}
}

static void
_et_command_cb (ETextEventProcessor *tep,
                ETextEventProcessorCommand *command,
                gpointer user_data)
{
	AtkObject *accessible;
	AtkText *text;

	accessible = ATK_OBJECT (user_data);
	text = ATK_TEXT (accessible);

	switch (command->action) {
	case E_TEP_MOVE:
		g_signal_emit_by_name (text, "text-caret-moved", et_get_caret_offset (text));
		break;
	case E_TEP_SELECT:
		g_signal_emit_by_name (text, "text-selection-changed");
		break;
	default:
		break;
	}
}

static void
et_real_initialize (AtkObject *obj,
                    gpointer data)
{
	EText *etext;

	ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);

	g_return_if_fail (GAL_A11Y_IS_E_TEXT (obj));
	g_return_if_fail (E_IS_TEXT (data));

	etext = E_TEXT (data);

	/* Set up signal callbacks */
	g_signal_connect (
		etext->model, "reposition",
		G_CALLBACK (_et_reposition_cb), obj);

	if (etext->tep)
		g_signal_connect_after (
			etext->tep, "command",
			(GCallback) _et_command_cb, obj);

	obj->role = ATK_ROLE_TEXT;
}

static void
et_class_init (GalA11yETextClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (class);

	quark_accessible_object =
		g_quark_from_static_string ("gtk-accessible-object");
	parent_class = g_type_class_ref (PARENT_TYPE);
	component_parent_iface =
		g_type_interface_peek (parent_class, ATK_TYPE_COMPONENT);
	object_class->dispose = et_dispose;
	atk_class->initialize = et_real_initialize;
}

static void
et_init (GalA11yEText *a11y)
{
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

		factory = atk_registry_get_factory (
			atk_get_default_registry (), GNOME_TYPE_CANVAS_ITEM);
		parent_type = atk_object_factory_get_accessible_type (factory);

		type = gal_a11y_type_register_static_with_private (
			PARENT_TYPE, "GalA11yEText", &info, 0,
			sizeof (GalA11yETextPrivate), &priv_offset);

		g_type_add_interface_static (
			type, ATK_TYPE_COMPONENT, &atk_component_info);
		g_type_add_interface_static (
			type, ATK_TYPE_TEXT, &atk_text_info);
		g_type_add_interface_static (
			type, ATK_TYPE_EDITABLE_TEXT, &atk_editable_text_info);
	}

	return type;
}

void
gal_a11y_e_text_init (void)
{
	if (atk_get_root ())
		atk_registry_set_factory_type (
			atk_get_default_registry (),
			E_TYPE_TEXT,
			gal_a11y_e_text_factory_get_type ());

}

