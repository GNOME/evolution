/*
 * e-summary-rdf.h:
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef __E_SUMMARY_RDF_H__
#define __E_SUMMARY_RDF_H__

#include "e-summary-type.h"

typedef struct _ESummaryRDF ESummaryRDF;

char *e_summary_rdf_get_html (ESummary *summary);
void e_summary_rdf_init (ESummary *summary);
#endif
