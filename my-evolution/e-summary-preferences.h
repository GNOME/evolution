/*
 * e-summary-preferences.h
 *
 * Copyright (C) 2001 Ximian, Inc
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

gboolean e_summary_preferences_restore (ESummaryPrefs *prefs);
void e_summary_preferences_save (ESummaryPrefs *prefs);
void e_summary_preferences_free (ESummaryPrefs *prefs);
ESummaryPrefs *e_summary_preferences_copy (ESummaryPrefs *prefs);
void e_summary_configure (GtkWidget *widget,
			  ESummary *summary);
void e_summary_preferences_init (ESummary *summary);
