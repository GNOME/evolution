/*
 * e-summary-preferences.h
 *
 * Copyright (C) 2001 Ximian, Inc
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef __E_SUMMARY_PREFERENCES_H__
#define __E_SUMMARY_PREFERENCES_H__

#include <bonobo/bonobo-ui-component.h>

gboolean e_summary_preferences_restore (ESummaryPrefs *prefs);
void e_summary_preferences_save (ESummaryPrefs *prefs);
void e_summary_preferences_free (ESummaryPrefs *prefs);
ESummaryPrefs *e_summary_preferences_copy (ESummaryPrefs *prefs);
void e_summary_configure (BonoboUIComponent *component,
			  gpointer userdata,
			  const char *cname);
void e_summary_preferences_init (ESummary *summary);

#endif
