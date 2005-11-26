/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* memos-control.c
 *
 * Copyright (C) 2000, 2001, 2002, 2003  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Damon Chaplin <damon@ximian.com>
 *	    Ettore Perazzoli
 *          Nathan Owens <pianocomp81@yahoo.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtksignal.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-paper.h>
#include <libgnomeprint/gnome-print-job.h>
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <libgnomeprintui/gnome-print-paper-selector.h>
#include <libgnomeprintui/gnome-print-preview.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-util.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-print.h>
#include <e-util/e-util-private.h>

#include "calendar-config.h"
#include "e-memos.h"
#include "e-memo-table.h"
#include "print.h"
#include "memos-control.h"
#include "evolution-shell-component-utils.h"

#define FIXED_MARGIN                            .05


static void memos_control_activate_cb		(BonoboControl		*control,
						 gboolean		 activate,
						 gpointer		 user_data);
static void memos_control_open_memo_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
static void memos_control_new_memo_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
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
						 const char		*path);
static void memos_control_print_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
static void memos_control_print_preview_cmd	(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);



BonoboControl *
memos_control_new (void)
{
	BonoboControl *control;
	GtkWidget *memos;

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
memos_control_sensitize_commands (BonoboControl *control, EMemos *memos, int n_selected)
{
	BonoboUIComponent *uic;
	gboolean read_only = TRUE;
	ECal *ecal;
	ECalModel *model;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	if (bonobo_ui_component_get_container (uic) == CORBA_OBJECT_NIL)
		return;

	model = e_memo_table_get_model (e_memos_get_calendar_table (memos));
	ecal = e_cal_model_get_default_client (model);
	if (ecal)
		e_cal_is_read_only (ecal, &read_only, NULL);

	bonobo_ui_component_set_prop (uic, "/commands/MemosOpenMemo", "sensitive",
				      n_selected != 1 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/MemosCut", "sensitive",
				      n_selected == 0 || read_only ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/MemosCopy", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/MemosPaste", "sensitive",
				      read_only ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/MemosDelete", "sensitive",
				      n_selected == 0 || read_only ? "0" : "1",
				      NULL);
}

/* Callback used when the selection in the table changes */
static void
selection_changed_cb (EMemos *memos, int n_selected, gpointer data)
{
	BonoboControl *control;

	control = BONOBO_CONTROL (data);

	memos_control_sensitize_commands (control, memos, n_selected);
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

void
memos_control_activate (BonoboControl *control, EMemos *memos)
{
	Bonobo_UIContainer remote_uih;
	BonoboUIComponent *uic;
	int n_selected;
	EMemoTable *cal_table;
	ETable *etable;
	char *xmlfile;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

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

	g_assert (uic != NULL);

	e_memos_set_ui_component (memos, NULL);

	e_memos_discard_view_menus (memos);

	/* Stop monitoring the "selection_changed" signal */
	g_signal_handlers_disconnect_matched (memos, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, control);

	bonobo_ui_component_rm (uic, "/", NULL);
 	bonobo_ui_component_unset_container (uic, NULL);
}

static void memos_control_open_memo_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path)
{
	EMemos *memos;

	memos = E_MEMOS (data);
	e_memos_open_memo (memos);
}

static void
memos_control_new_memo_cmd		(BonoboUIComponent	*uic,
					 gpointer		 data,
					 const char		*path)
{
	EMemos *memos;

	memos = E_MEMOS (data);
	e_memos_new_memo (memos);
}

static void
memos_control_cut_cmd                   (BonoboUIComponent      *uic,
					 gpointer                data,
					 const char             *path)
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
					 const char             *path)
{
	EMemos *memos;
	EMemoTable *cal_table;

	memos = E_MEMOS (data);
	cal_table = e_memos_get_calendar_table (memos);
	e_memo_table_copy_clipboard (cal_table);
}

static void
memos_control_paste_cmd                 (BonoboUIComponent      *uic,
					 gpointer                data,
					 const char             *path)
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
					 const char		*path)
{
	EMemos *memos;

	memos = E_MEMOS (data);
	e_memos_delete_selected (memos);
}


static void
print_memos (EMemos *memos, gboolean preview)
{
	EMemoTable *cal_table;
	ETable *etable;

	cal_table = e_memos_get_calendar_table (memos);
	etable = e_memo_table_get_table (E_MEMO_TABLE (cal_table));

	print_table (etable, _("Print Memos"), _("Memos"), preview);
}

/* File/Print callback */
static void
memos_control_print_cmd (BonoboUIComponent *uic,
			 gpointer data,
			 const char *path)
{
	EMemos *memos;

	memos = E_MEMOS (data);

	print_memos (memos, FALSE);
}

static void
memos_control_print_preview_cmd (BonoboUIComponent *uic,
				 gpointer data,
				 const char *path)
{
	EMemos *memos;

	memos = E_MEMOS (data);

	print_memos (memos, TRUE);
}

