/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_UTILS_H_
#define _E_TABLE_UTILS_H_

#include <gal/e-table/e-table-header.h>
#include <gal/e-table/e-table-state.h>
#include <gal/e-table/e-table-specification.h>
#include <gal/e-table/e-table-extras.h>

G_BEGIN_DECLS

ETableHeader *
e_table_state_to_header (GtkWidget *widget, ETableHeader *full_header, ETableState *state);

ETableHeader *
e_table_spec_to_full_header (ETableSpecification *spec,
			     ETableExtras        *ete);

G_END_DECLS

#endif /* _E_TABLE_UTILS_H_ */

