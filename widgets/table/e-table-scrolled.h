/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SCROLLED_H_
#define _E_TABLE_SCROLLED_H_

#include <gal/widgets/e-scroll-frame.h>
#include <gal/e-table/e-table-model.h>
#include <gal/e-table/e-table-header.h>
#include <gal/e-table/e-table.h>
#include <gal/widgets/e-printable.h>

BEGIN_GNOME_DECLS

#define E_TABLE_SCROLLED_TYPE        (e_table_scrolled_get_type ())
#define E_TABLE_SCROLLED(o)          (GTK_CHECK_CAST ((o), E_TABLE_SCROLLED_TYPE, ETableScrolled))
#define E_TABLE_SCROLLED_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SCROLLED_TYPE, ETableScrolledClass))
#define E_IS_TABLE_SCROLLED(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SCROLLED_TYPE))
#define E_IS_TABLE_SCROLLED_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SCROLLED_TYPE))

typedef struct {
	EScrollFrame parent;

	ETable *table;
} ETableScrolled;

typedef struct {
	EScrollFrameClass parent_class;
} ETableScrolledClass;

GtkType         e_table_scrolled_get_type                  (void);

ETableScrolled *e_table_scrolled_construct                 (ETableScrolled *ets,
							    ETableModel    *etm,
							    ETableExtras   *ete,
							    const char     *spec,
							    const char     *state);
GtkWidget      *e_table_scrolled_new                       (ETableModel    *etm,
							    ETableExtras   *ete,
							    const char     *spec,
							    const char     *state);

ETableScrolled *e_table_scrolled_construct_from_spec_file  (ETableScrolled *ets,
							    ETableModel    *etm,
							    ETableExtras   *ete,
							    const char     *spec_fn,
							    const char     *state_fn);
GtkWidget      *e_table_scrolled_new_from_spec_file        (ETableModel    *etm,
							    ETableExtras   *ete,
							    const char     *spec_fn,
							    const char     *state_fn);

ETable         *e_table_scrolled_get_table                 (ETableScrolled *ets);

END_GNOME_DECLS

#endif /* _E_TABLE_SCROLLED_H_ */

