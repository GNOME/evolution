/*
 * Text cell renderer.
 *
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
 *		Miguel de Icaza <miguel@ximian.com>
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * A lot of code taken from:
 *
 * Text item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent
 * canvas widget.  Tk is copyrighted by the Regents of the University
 * of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CELL_TEXT_H
#define E_CELL_TEXT_H

#include <gtk/gtk.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-cell.h>

/* Standard GObject macros */
#define E_TYPE_CELL_TEXT \
	(e_cell_text_get_type ())
#define E_CELL_TEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_TEXT, ECellText))
#define E_CELL_TEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_TEXT, ECellTextClass))
#define E_IS_CELL_TEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_TEXT))
#define E_IS_CELL_TEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_TEXT))
#define E_CELL_TEXT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_TEXT, ECellTextClass))

G_BEGIN_DECLS

typedef struct _ECellText ECellText;
typedef struct _ECellTextClass ECellTextClass;

struct _ECellText {
	ECell parent;

	GtkJustification  justify;
	gchar             *font_name;

	gdouble x, y;			/* Position at anchor */

	gulong pixel;			/* Fill color */

	/* Clip handling */
	gchar *ellipsis;                 /* The ellipsis characters.  NULL = "...". */

	guint use_ellipsis : 1;         /* Whether to use the ellipsis. */
	guint editable : 1;		/* Whether the text can be edited. */
	guint use_tabular_numbers : 1;  /* Whether to use the tabular numbers. */
	guint is_markup : 1;            /* Whether the text is markup. */

	gint strikeout_column;
	gint underline_column;
	gint bold_column;
	gint italic_column;
	gint strikeout_color_column;

	/* This column in the ETable should return a string specifying a color,
	 * either a color name like "red" or a color spec like "rgb:F/0/0".
	 * See the XParseColor man page for the formats available. */
	gint color_column;
	gint bg_color_column;
};

struct _ECellTextClass {
	ECellClass parent_class;

	/* Methods */
	gchar *		(*get_text)		(ECellText *cell,
						 ETableModel *model,
						 gint col,
						 gint row);
	void		(*free_text)		(ECellText *cell,
						 ETableModel *model,
						 gint col,
						 gchar *text);
	void		(*set_value)		(ECellText *cell,
						 ETableModel *model,
						 gint col,
						 gint row,
						 const gchar *text);

	/* Signals */
	void		(*text_inserted)	(ECellText *cell,
						 ECellView *cell_view,
						 gint pos,
						 gint len,
						 gint row,
						 gint model_col);
	void		(*text_deleted)		(ECellText *cell,
						 ECellView *cell_view,
						 gint pos,
						 gint len,
						 gint row,
						 gint model_col);
};

GType		e_cell_text_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_text_new			(const gchar *fontname,
						 GtkJustification justify);
ECell *		e_cell_text_construct		(ECellText *cell,
						 const gchar *fontname,
						 GtkJustification justify);

/* Gets the value from the model and converts it into a string. In ECellText
 * itself, the value is assumed to be a gchar * and so needs no conversion.
 * In subclasses the ETableModel value may be a more complicated datatype. */
gchar *		e_cell_text_get_text		(ECellText *cell,
						 ETableModel *model,
						 gint col,
						 gint row);

/* Frees the value returned by e_cell_text_get_text(). */
void		e_cell_text_free_text		(ECellText *cell,
						 ETableModel *model,
						 gint col,
						 gchar *text);

/* Sets the ETableModel value, based on the given string. */
void		e_cell_text_set_value		(ECellText *cell,
						 ETableModel *model,
						 gint col,
						 gint row,
						 const gchar *text);

/* Sets the selection of given text cell */
gboolean	e_cell_text_set_selection	(ECellView *cell_view,
						 gint col,
						 gint row,
						 gint start,
						 gint end);

/* Gets the selection of given text cell */
gboolean	e_cell_text_get_selection	(ECellView *cell_view,
						 gint col,
						 gint row,
						 gint *start,
						 gint *end);

/* Copys the selected text to the clipboard */
void		e_cell_text_copy_clipboard	(ECellView *cell_view,
						 gint col,
						 gint row);

/* Pastes the text from the clipboard */
void		e_cell_text_paste_clipboard	(ECellView *cell_view,
						 gint col,
						 gint row);

/* Deletes selected text */
void		e_cell_text_delete_selection	(ECellView *cell_view,
						 gint col,
						 gint row);

/* get text directly from view, both col and row are model format */
gchar *		e_cell_text_get_text_by_view	(ECellView *cell_view,
						 gint col,
						 gint row);

PangoEllipsizeMode
		e_cell_text_get_ellipsize_mode	(ECellText *self);
void		e_cell_text_set_ellipsize_mode	(ECellText *self,
						 PangoEllipsizeMode mode);

G_END_DECLS

#endif /* E_CELL_TEXT_H */

