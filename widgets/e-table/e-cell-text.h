/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ECellText - Text item for e-table.
 * Copyright (C) 2000 Helix Code, Inc.
 * Author: Chris Lahey <clahey@umich.edu>
 *
 * Drawing and event handling from:
 * 
 * EText - Text item for evolution.
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Chris Lahey <clahey@umich.edu>
 *
 * A majority of code taken from:
 *
 * Text item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent
 * canvas widget.  Tk is copyrighted by the Regents of the University
 * of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx> */
#ifndef _E_CELL_TEXT_H_
#define _E_CELL_TEXT_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-cell.h"
#include "e-text-event-processor.h"

#define E_CELL_TEXT_TYPE        (e_cell_text_get_type ())
#define E_CELL_TEXT(o)          (GTK_CHECK_CAST ((o), E_CELL_TEXT_TYPE, ECellText))
#define E_CELL_TEXT_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_TEXT_TYPE, ECellTextClass))
#define E_IS_CELL_TEXT(o)       (GTK_CHECK_TYPE ((o), E_CELL_TEXT_TYPE))
#define E_IS_CELL_TEXT_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_TEXT_TYPE))

typedef struct {
	ECell parent;

	GtkJustification  justify;
	char             *font_name;

	double x, y;			/* Position at anchor */

	gulong pixel;			/* Fill color */

	/* Clip handling */
	char *ellipsis;                 /* The ellipsis characters.  NULL = "...". */

	guint use_ellipsis : 1;         /* Whether to use the ellipsis. */
} ECellText;

typedef struct {
	ECellClass parent_class;
} ECellTextClass;

GtkType    e_cell_text_get_type (void);
ECell     *e_cell_text_new      (ETableModel *model, const char *fontname, GtkJustification justify);

#endif /* _E_CELL_TEXT_H_ */


