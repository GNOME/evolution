/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *	    Ettore Perazzoli
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-util.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>
#include <e-util/e-print.h>
#include <e-util/e-util-private.h>
#include <gtkhtml/gtkhtml.h>

#include "calendar-config.h"
#include "e-memos.h"
#include "e-memo-table.h"
#include "print.h"
#include "memos-control.h"
#include "e-cal-component-memo-preview.h"
#include "evolution-shell-component-utils.h"

#define FIXED_MARGIN                            .05

static void memos_control_activate_cb		(BonoboControl		*control,
						 gboolean		 activate,
						 gpointer		 user_data);
static void memos_control_open_memo_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const gchar		*path);
static void memos_control_new_memo_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const gchar		*path);
static void memos_control_cut_cmd               (BonoboUIComponent      *uic,
						 gpointer                data,
						 const gchar            *path);
static void memos_control_copy_cmd              (BonoboUIComponent      *uic,
						 gpointer                data,
						 const gchar            *path);
static void memos_control_paste_cmd             (BonoboUIComponent      *uic,
						 gpointer                data,
						 const gchar            *path);
static void memos_control_delete_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const gchar		*path);
static void memos_control_print_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const gchar		*path);
static void memos_control_print_preview_cmd	(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const gchar		*path);

struct focus_changed_data {
	BonoboControl *control;
	EMemos *memos;
};

static gboolean memos_control_focus_changed (GtkWidget *widget, GdkEventFocus *event, struct focus_changed_data *fc_data);

BonoboControl *
memos_control_new (void)
{
	BonoboControl *control;
	GtkWidget *memos, *preview;
	struct focus_changed_data *fc_data;

	memos = e_memos_new ();
	if (!memos)
		return NULL;
	gtk_widget_show (memos);

	control = bonobo_control_new (memos);
	if (!control) {
		gtk_widget_destroy (memos);
		g_message ("control_factory_fn(): could not create the control!");
		return NULL;
	}

	g_signal_connect (control, "activate", G_CALLBACK (memos_control_activate_cb), memos);

	fc_data = g_new0 (struct focus_changed_data, 1);
	fc_data->control = control;
	fc_data->memos = E_MEMOS (memos);

	preview = e_cal_component_memo_preview_get_html (E_CAL_COMPONENT_MEMO_PREVIEW (e_memos_get_preview (fc_data->memos)));
	g_object_set_data_full (G_OBJECT (preview), "memos-ctrl-fc-data", fc_data, g_free);
	g_signal_connect (preview, "focus-in-event", G_CALLBACK (memos_control_focus_changed), fc_data);
	g_signal_connect (preview, "focus-out-event", G_CALLBACK (memos_control_focus_changed), fc_data);

	return control;
}

static void
memos_control_activate_cb		(BonoboControl		*control,
					 gboolean		 activate,
					 gpointer		 user_data)
{
	EMemos *memos;

	memos = E_MEMOS (user_data);

	if (activate)
		memos_control_activate (control, memos);
	else
		memos_control_deactivate (control, memos);
}

/* Sensitizes the UI Component menu/toolbar commands based on the number of
 * selected memos.
 */
void
memos_control_sensitize_commands (BonoboControl *control, EMemos *memos, gint n_selected)
{
	BonoboUIComponent *uic;
	gboolean read_only = TRUE, preview_active;
	ECal *ecal;
	ECalModel *model;
	GtkWidget *preview;

	uic = bonobo_control_get_ui_component (control);
	g_return_if_fail (uic != NULL);

	if (bonobo_ui_component_get_container (uic) == CORBA_OBJECT_NIL)
		return;

	preview = e_cal_component_memo_preview_get_html (E_CAL_COMPONENT_MEMO_PREVIEW (e_memos_get_preview (memos)));
	preview_active = preview && GTK_WIDGET_VISIBLE (preview) && GTK_WIDGET_HAS_FOCUS (preview);

	model = e_memo_table_get_model (e_memos_get_calendar_table (memos));
	ecal = e_cal_model_get_default_client (model);
	if (ecal)
		e_cal_is_read_only (ecal, &read_only, NULL);

	bonobo_ui_component_set_prop (uic, "/commands/MemosOpenMemo", "sensitive",
				      n_selected != 1 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/MemosCut", "sensitive",
				      n_selected == 0 || read_only || preview_active ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/MemosCopy", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/MemosPaste", "sensitive",
				      read_only || preview_active ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/MemosDelete", "sensitive",
				      n_selected == 0 || read_only ? "0" : "1",
				      NULL);
}

/* Callback used when the selection in the table changes */
static void
selection_changed_cb (EMemos *memos, gint n_selected, gpointer data)
{
	BonoboControl *control;

	control = BONOBO_CONTROL (data);

	memos_control_sensitize_commands (control, memos, n_selected);
}

static gboolean
memos_control_focus_changed (GtkWidget *widget, GdkEventFocus *event, struct focus_changed_data *fc_data)
{
	g_return_val_if_fail (fc_data != NULL, FALSE);

	memos_control_sensitize_commands (fc_data->control, fc_data->memos, e_table_selected_count (e_memo_table_get_table (e_memos_get_calendar_table (fc_data->memos))));

	return FALSE;
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("MemosOpenMemo", memos_control_open_memo_cmd),
	BONOBO_UI_VERB ("MemosNewMemo", memos_control_new_memo_cmd),
	BONOBO_UI_VERB ("MemosCut", memos_control_cut_cmd),
	BONOBO_UI_VERB ("MemosCopy", memos_control_copy_cmd),
	BONOBO_UI_VERB ("MemosPaste", memos_control_paste_cmd),
	BONOBO_UI_VERB ("MemosDelete", memos_control_delete_cmd),
	BONOBO_UI_VERB ("MemosPrint", memos_control_print_cmd),
	BONOBO_UI_VERB ("MemosPrintPreview", memos_control_print_preview_cmd),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/commands/MemosCopy", "edit-copy", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MemosCut", "edit-cut", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MemosDelete", "edit-delete", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MemosPaste", "edit-paste", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MemosPrint", "document-print", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/MemosPrintPreview", "document-print-preview", GTK_ICON_SIZE_MENU),

	E_PIXMAP ("/Toolbar/Cut", "edit-cut", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/Copy", "edit-copy", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/Paste", "edit-paste", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/Print", "document-print", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/Delete", "edit-delete", GTK_ICON_SIZE_LARGE_TOOLBAR),

	E_PIXMAP_END
};

void
memos_control_activate (BonoboControl *control, EMemos *memos)
{
	Bonobo_UIContainer remote_uih;
	BonoboUIComponent *uic;
	gint n_selected;
	EMemoTable *cal_table;
	ETable *etable;
	gchar *xmlfile;

	uic = bonobo_control_get_ui_component (control);
	g_return_if_fail (uic != NULL);

	remote_uih = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, remote_uih, NULL);
	bonobo_object_release_unref (remote_uih, NULL);

	e_memos_set_ui_component (memos, uic);

	bonobo_ui_component_add_verb_list_with_data (uic, verbs, memos);

	bonobo_ui_component_freeze (uic, NULL);

	xmlfile = g_build_filename (EVOLUTION_UIDIR,
				    "evolution-memos.xml",
				    NULL);
	bonobo_ui_util_set_ui (uic, PREFIX,
			       xmlfile,
			       "evolution-memos",
			       NULL);
	g_free (xmlfile);

	e_pixmaps_update (uic, pixmaps);

	e_memos_setup_view_menus (memos, uic);

	/* Signals from the memos widget; also sensitize the menu items as appropriate */

	g_signal_connect (memos, "selection_changed", G_CALLBACK (selection_changed_cb), control);

	cal_table = e_memos_get_calendar_table (memos);
	etable = e_memo_table_get_table (cal_table);
	n_selected = e_table_selected_count (etable);

	memos_control_sensitize_commands (control, memos, n_selected);

	bonobo_ui_component_thaw (uic, NULL);
}

void
memos_control_deactivate (BonoboControl *control, EMemos *memos)
{
	BonoboUIComponent *uic = bonobo_control_get_ui_component (control);

	g_return_if_fail (uic != NULL);

	e_memos_set_ui_component (memos, NULL);

	e_memos_discard_view_menus (memos);

	/* Stop monitoring the "selection_changed" signal */
	g_signal_handlers_disconnect_matched (memos, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, control);

	bonobo_ui_component_rm (uic, "/", NULL);
	bonobo_ui_component_unset_container (uic, NULL);
}

static void memos_control_open_memo_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const gchar		*path)
{
	EMemos *memos;

	memos = E_MEMOS (data);
	e_memos_open_memo (memos);
}

static void
memos_control_new_memo_cmd		(BonoboUIComponent	*uic,
					 gpointer		 data,
					 const gchar		*path)
{
	EMemos *memos;

	memos = E_MEMOS (data);
	e_memos_new_memo (memos);
}

static void
memos_control_cut_cmd                   (BonoboUIComponent      *uic,
					 gpointer                data,
					 const gchar             *path)
{
	EMemos *memos;
	EMemoTable *cal_table;

	memos = E_MEMOS (data);
	cal_table = e_memos_get_calendar_table (memos);
	e_memo_table_cut_clipboard (cal_table);
}

static void
memos_control_copy_cmd                  (BonoboUIComponent      *uic,
					 gpointer                data,
					 const gchar             *path)
{
	EMemos *memos;
	EMemoTable *cal_table;
	GtkWidget *preview;

	memos = E_MEMOS (data);

	preview = e_cal_component_memo_preview_get_html (E_CAL_COMPONENT_MEMO_PREVIEW (e_memos_get_preview (memos)));
	if (preview && GTK_WIDGET_VISIBLE (preview) && GTK_WIDGET_HAS_FOCUS (preview)) {
		/* copy selected text in a preview when that's shown and focused */
		gtk_html_copy (GTK_HTML (preview));
	} else {
		cal_table = e_memos_get_calendar_table (memos);
		e_memo_table_copy_clipboard (cal_table);
	}
}

static void
memos_control_paste_cmd                 (BonoboUIComponent      *uic,
					 gpointer                data,
					 const gchar             *path)
{
	EMemos *memos;
	EMemoTable *cal_table;

	memos = E_MEMOS (data);
	cal_table = e_memos_get_calendar_table (memos);
	e_memo_table_paste_clipboard (cal_table);
}

static void
memos_control_delete_cmd		(BonoboUIComponent	*uic,
					 gpointer		 data,
					 const gchar		*path)
{
	EMemos *memos;

	memos = E_MEMOS (data);
	e_memos_delete_selected (memos);
}

/* File/Print callback */
static void
memos_control_print_cmd (BonoboUIComponent *uic,
			 gpointer data,
			 const gchar *path)
{
	EMemos *memos = E_MEMOS (data);
	ETable *table;

	table = e_memo_table_get_table (
		E_MEMO_TABLE (e_memos_get_calendar_table (memos)));

	print_table (
		table, _("Print Memos"), _("Memos"),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
memos_control_print_preview_cmd (BonoboUIComponent *uic,
				 gpointer data,
				 const gchar *path)
{
	EMemos *memos = E_MEMOS (data);
	ETable *table;

	table = e_memo_table_get_table (
		E_MEMO_TABLE (e_memos_get_calendar_table (memos)));

	print_table (
		table, _("Print Memos"), _("Memos"),
		GTK_PRINT_OPERATION_ACTION_PREVIEW);
}
