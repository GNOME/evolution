/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "e-util/e-util.h"

#include "select-source-dialog.h"

/**
 * select_source_dialog
 *
 * Implements dialog for allowing user to select a destination source.
 */
ESource *
select_source_dialog (GtkWindow *parent,
                      ESourceRegistry *registry,
                      ECalClientSourceType obj_type,
                      ESource *except_source)
{
	GtkWidget *dialog;
	ESource *selected_source = NULL;
	const gchar *extension_name;
	const gchar *icon_name;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	if (obj_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
		extension_name = E_SOURCE_EXTENSION_CALENDAR;
		icon_name = "x-office-calendar";
	} else if (obj_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS) {
		extension_name = E_SOURCE_EXTENSION_TASK_LIST;
		icon_name = "stock_todo";
	} else if (obj_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS) {
		extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
		icon_name = "stock_journal";
	} else
		return NULL;

	/* create the dialog */
	dialog = e_source_selector_dialog_new (parent, registry, extension_name);

	if (icon_name)
		gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	if (except_source)
		e_source_selector_dialog_set_except_source (E_SOURCE_SELECTOR_DIALOG (dialog), except_source);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	selected_source = e_source_selector_dialog_peek_primary_selection (
		E_SOURCE_SELECTOR_DIALOG (dialog));
	if (selected_source != NULL)
		g_object_ref (selected_source);

exit:
	gtk_widget_destroy (dialog);

	return selected_source;
}
