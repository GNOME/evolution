/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ECellNumber - Number item for e-table.
 * Copyright (C) 2001 Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 */
#ifndef _E_CELL_NUMBER_H_
#define _E_CELL_NUMBER_H_

#include <gal/e-table/e-cell-text.h>

BEGIN_GNOME_DECLS

#define E_CELL_NUMBER_TYPE        (e_cell_number_get_type ())
#define E_CELL_NUMBER(o)          (GTK_CHECK_CAST ((o), E_CELL_NUMBER_TYPE, ECellNumber))
#define E_CELL_NUMBER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_NUMBER_TYPE, ECellNumberClass))
#define E_IS_CELL_NUMBER(o)       (GTK_CHECK_TYPE ((o), E_CELL_NUMBER_TYPE))
#define E_IS_CELL_NUMBER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_NUMBER_TYPE))

typedef struct {
	ECellText base;
} ECellNumber;

typedef struct {
	ECellTextClass parent_class;
} ECellNumberClass;

GtkType    e_cell_number_get_type (void);
ECell     *e_cell_number_new      (const char *fontname, GtkJustification justify);

END_GNOME_DECLS

#endif /* _E_CELL_NUMBER_H_ */
