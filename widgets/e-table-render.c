/*
 * E-table-render.c: Various renderers
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, Helix Code, Inc.
 */
#include <config.h>
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-col.h"
#include "e-table-render.h"

void
e_table_render_string (ERenderContext *ctxt)
{
	printf ("Rendering string: %s\n", ctxt->render_data);
}

