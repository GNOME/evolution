/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ECellDate - Date item for e-table.
 * Copyright (C) 2001 Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 */
#ifndef _E_CELL_DATE_H_
#define _E_CELL_DATE_H_

#include <gal/e-table/e-cell-text.h>

BEGIN_GNOME_DECLS

#define E_CELL_DATE_TYPE        (e_cell_date_get_type ())
#define E_CELL_DATE(o)          (GTK_CHECK_CAST ((o), E_CELL_DATE_TYPE, ECellDate))
#define E_CELL_DATE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_DATE_TYPE, ECellDateClass))
#define E_IS_CELL_DATE(o)       (GTK_CHECK_TYPE ((o), E_CELL_DATE_TYPE))
#define E_IS_CELL_DATE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_DATE_TYPE))

typedef struct {
	ECellText base;
} ECellDate;

typedef struct {
	ECellTextClass parent_class;
} ECellDateClass;

GtkType    e_cell_date_get_type (void);
ECell     *e_cell_date_new      (const char *fontname, GtkJustification justify);

END_GNOME_DECLS

#endif /* _E_CELL_DATE_H_ */
