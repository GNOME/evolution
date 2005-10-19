/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Memo editor dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>

#include "memo-page.h"
#include "cancel-comp.h"
#include "memo-editor.h"

struct _MemoEditorPrivate {
	MemoPage *memo_page;
	
	gboolean updating;	
};

static void memo_editor_set_e_cal (CompEditor *editor, ECal *client);
static void memo_editor_edit_comp (CompEditor *editor, ECalComponent *comp);
static gboolean memo_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method);
static void memo_editor_finalize (GObject *object);

static void refresh_memo_cmd (GtkWidget *widget, gpointer data);
static void cancel_memo_cmd (GtkWidget *widget, gpointer data);
static void forward_cmd (GtkWidget *widget, gpointer data);

static void model_row_change_insert_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void model_row_delete_cb (GtkTreeModel *model, GtkTreePath *path, gpointer data);

G_DEFINE_TYPE (MemoEditor, memo_editor, TYPE_COMP_EDITOR);



/**
 * memo_editor_get_type:
 *
 * Registers the #MemoEditor class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #MemoEditor class.
 **/

/* Class initialization function for the event editor */
static void
memo_editor_class_init (MemoEditorClass *klass)
{
	GObjectClass *object_class;
	CompEditorClass *editor_class;

	object_class = (GObjectClass *) klass;
	editor_class = (CompEditorClass *) klass;

	editor_class->set_e_cal = memo_editor_set_e_cal;
	editor_class->edit_comp = memo_editor_edit_comp;
	editor_class->send_comp = memo_editor_send_comp;

	object_class->finalize = memo_editor_finalize;
}

static void
init_widgets (MemoEditor *me)
{
	MemoEditorPrivate *priv;

	priv = me->priv;
}

static void
client_changed_cb (CompEditorPage *page, ECal *client, gpointer user_data)
{
/*	set_menu_sens (MEMO_EDITOR (user_data)); */
}

/* Object initialization function for the memo editor */
static void
memo_editor_init (MemoEditor *te)
{
	MemoEditorPrivate *priv;
	
	priv = g_new0 (MemoEditorPrivate, 1);
	te->priv = priv;

	priv->updating = FALSE;	

	/* TODO add help stuff */
/*	comp_editor_set_help_section (COMP_EDITOR (te), "usage-calendar-memo"); */
}

MemoEditor *
memo_editor_construct (MemoEditor *me, ECal *client)
{
	MemoEditorPrivate *priv;
	
	gboolean read_only = FALSE;
	
	priv = me->priv;

	priv->memo_page = memo_page_new ();
	g_object_ref (priv->memo_page);
	gtk_object_sink (GTK_OBJECT (priv->memo_page));
	comp_editor_append_page (COMP_EDITOR (me), 
				 COMP_EDITOR_PAGE (priv->memo_page),
				 _("Memo"));
	g_signal_connect (G_OBJECT (priv->memo_page), "client_changed",
			  G_CALLBACK (client_changed_cb), me);

	if (!e_cal_is_read_only (client, &read_only, NULL))
		read_only = TRUE;

	comp_editor_set_e_cal (COMP_EDITOR (me), client);

	init_widgets (me);

	return me;
}

static void
memo_editor_set_e_cal (CompEditor *editor, ECal *client)
{
	MemoEditor *te;
	MemoEditorPrivate *priv;

	te = MEMO_EDITOR (editor);
	priv = te->priv;

	if (COMP_EDITOR_CLASS (memo_editor_parent_class)->set_e_cal)
		COMP_EDITOR_CLASS (memo_editor_parent_class)->set_e_cal (editor, client);
}

static void
memo_editor_edit_comp (CompEditor *editor, ECalComponent *comp)
{
	MemoEditor *me;
	MemoEditorPrivate *priv;
	ECalComponentOrganizer organizer;
	ECal *client;
	
	me = MEMO_EDITOR (editor);
	priv = me->priv;

	priv->updating = TRUE;

	if (COMP_EDITOR_CLASS (memo_editor_parent_class)->edit_comp)
		COMP_EDITOR_CLASS (memo_editor_parent_class)->edit_comp (editor, comp);

	client = comp_editor_get_e_cal (COMP_EDITOR (editor));

	priv->updating = FALSE;
}

static gboolean
memo_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method)
{
	MemoEditor *me = MEMO_EDITOR (editor);
	MemoEditorPrivate *priv;
	ECalComponent *comp = NULL;

	priv = me->priv;

	if (COMP_EDITOR_CLASS (memo_editor_parent_class)->send_comp)
		return COMP_EDITOR_CLASS (memo_editor_parent_class)->send_comp (editor, method);

	return FALSE;
}

/* Destroy handler for the event editor */
static void
memo_editor_finalize (GObject *object)
{
	MemoEditor *me;
	MemoEditorPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MEMO_EDITOR (object));

	me = MEMO_EDITOR (object);
	priv = me->priv;

	if (priv->memo_page) {
		g_object_unref (priv->memo_page);
		priv->memo_page = NULL;
	}
	
	g_free (priv);

	if (G_OBJECT_CLASS (memo_editor_parent_class)->finalize)
		(* G_OBJECT_CLASS (memo_editor_parent_class)->finalize) (object);
}

/**
 * memo_editor_new:
 * @client: an ECal
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
MemoEditor *
memo_editor_new (ECal *client)
{
	MemoEditor *me;

	me = g_object_new (TYPE_MEMO_EDITOR, NULL);
	return memo_editor_construct (me, client);
}

static void
refresh_memo_cmd (GtkWidget *widget, gpointer data)
{
	MemoEditor *me = MEMO_EDITOR (data);

	comp_editor_send_comp (COMP_EDITOR (me), E_CAL_COMPONENT_METHOD_REFRESH);
}

static void
cancel_memo_cmd (GtkWidget *widget, gpointer data)
{
	MemoEditor *me = MEMO_EDITOR (data);
	ECalComponent *comp;
	
	comp = comp_editor_get_current_comp (COMP_EDITOR (me));
	if (cancel_component_dialog ((GtkWindow *) me,
				     comp_editor_get_e_cal (COMP_EDITOR (me)), comp, FALSE)) {
		comp_editor_send_comp (COMP_EDITOR (me), E_CAL_COMPONENT_METHOD_CANCEL);
		comp_editor_delete_comp (COMP_EDITOR (me));
	}
}

static void
forward_cmd (GtkWidget *widget, gpointer data)
{
	MemoEditor *me = MEMO_EDITOR (data);
	
	if (comp_editor_save_comp (COMP_EDITOR (me), TRUE))
		comp_editor_send_comp (COMP_EDITOR (me), E_CAL_COMPONENT_METHOD_PUBLISH);
}

static void
model_changed (MemoEditor *me)
{
	if (!me->priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (me), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (me), TRUE);
	}	
}

static void
model_row_change_insert_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	model_changed (MEMO_EDITOR (data));
}

static void
model_row_delete_cb (GtkTreeModel *model, GtkTreePath *path, gpointer data)
{
	model_changed (MEMO_EDITOR (data));
}
