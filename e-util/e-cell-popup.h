/*
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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellPopup - an ECell used to support popup selections like a GtkCombo
 * widget. It contains a child ECell, e.g. an ECellText, but when selected it
 * displays an arrow on the right edge which the user can click to show a
 * popup. It will support subclassing or signals so that different types of
 * popup can be provided.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_CELL_POPUP_H_
#define _E_CELL_POPUP_H_

#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-cell.h>

/* Standard GObject macros */
#define E_TYPE_CELL_POPUP \
	(e_cell_popup_get_type ())
#define E_CELL_POPUP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_POPUP, ECellPopup))
#define E_CELL_POPUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_POPUP, ECellPopupClass))
#define E_IS_CELL_POPUP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_POPUP))
#define E_IS_CELL_POPUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_POPUP))
#define E_CELL_POPUP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_POPUP, ECellPopupClass))

G_BEGIN_DECLS

typedef struct _ECellPopup ECellPopup;
typedef struct _ECellPopupView ECellPopupView;
typedef struct _ECellPopupClass ECellPopupClass;

struct _ECellPopup {
	ECell parent;

	ECell *child;

	/* This is TRUE if the popup window is shown for the cell being
	 * edited. While shown we display the arrow indented. */
	gboolean popup_shown;

	/* This is TRUE if the popup arrow is shown for the cell being edited.
	 * This is needed to stop the first click on the cell from popping up
	 * the popup window. We only popup the window after we have drawn the
	 * arrow. */
	gboolean popup_arrow_shown;

	/* The view in which the popup is shown. */
	ECellPopupView *popup_cell_view;

	gint popup_view_col;
	gint popup_row;
	ETableModel *popup_model;
};

struct _ECellPopupView {
	ECellView cell_view;

	ECellView *child_view;
};

struct _ECellPopupClass {
	ECellClass parent_class;

	/* Virtual function for subclasses to override. */
	gint		(*popup)		(ECellPopup *ecp,
						 GdkEvent *event,
						 gint row,
						 gint view_col);
};

GType		e_cell_popup_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_popup_new		(void);
ECell *		e_cell_popup_get_child		(ECellPopup *ecp);
void		e_cell_popup_set_child		(ECellPopup *ecp,
						 ECell *child);
void		e_cell_popup_set_shown		(ECellPopup *ecp,
						 gboolean shown);
void		e_cell_popup_queue_cell_redraw	(ECellPopup *ecp);

G_END_DECLS

#endif /* _E_CELL_POPUP_H_ */
