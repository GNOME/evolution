/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ETableTextModel - Text item for evolution.
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

#ifndef E_TABLE_TEXT_MODEL_H
#define E_TABLE_TEXT_MODEL_H

#include <gnome.h>
#include "e-text-model.h"
#include "e-table-model.h"


BEGIN_GNOME_DECLS

#define E_TYPE_TABLE_TEXT_MODEL            (e_table_text_model_get_type ())
#define E_TABLE_TEXT_MODEL(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_TABLE_TEXT_MODEL, ETableTextModel))
#define E_TABLE_TEXT_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_TABLE_TEXT_MODEL, ETableTextModelClass))
#define E_IS_TABLE_TEXT_MODEL(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_TABLE_TEXT_MODEL))
#define E_IS_TABLE_TEXT_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_TABLE_TEXT_MODEL))

typedef struct _ETableTextModel ETableTextModel;
typedef struct _ETableTextModelClass ETableTextModelClass;

struct _ETableTextModel {
	ETextModel parent;

	ETableModel *model;
	int row;
	int model_col;

	int cell_changed_signal_id;
	int row_changed_signal_id;
};

struct _ETableTextModelClass {
	ETextModelClass parent_class;

};


/* Standard Gtk function */
GtkType e_table_text_model_get_type (void);
ETableTextModel *e_table_text_model_new (ETableModel *table_model, int row, int model_col);

END_GNOME_DECLS

#endif
