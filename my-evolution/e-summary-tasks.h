/*
 * e-summary-tasks.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef __E_SUMMARY_TASKS_H__
#define __E_SUMMARY_TASKS_H__

#include "e-summary-type.h"

typedef struct _ESummaryTasks ESummaryTasks;

const char *e_summary_tasks_get_html (ESummary *summary);
void e_summary_tasks_init (ESummary *summary);
void e_summary_tasks_reconfigure (ESummary *summary);
void e_summary_tasks_free (ESummary *summary);

#endif
