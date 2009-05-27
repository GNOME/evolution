/*
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <gconf/gconf-client.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserverui/e-source-selector.h>
#include <shell/e-user-creatable-items-handler.h>
#include <shell/e-component-view.h>
#include "e-cal-model.h"
#include "e-memos.h"
#include "memos-component.h"
#include "memos-control.h"
#include "e-comp-editor-registry.h"
#include "migration.h"
#include "comp-util.h"
#include "calendar-config.h"
#include "e-cal-popup.h"
#include "common/authentication.h"
#include "dialogs/calendar-setup.h"
#include "dialogs/comp-editor.h"
#include "dialogs/copy-source-dialog.h"
#include "dialogs/memo-editor.h"
#include "widgets/misc/e-info-label.h"
#include "e-util/e-error.h"
#include "calendar-component.h"

#define CREATE_MEMO_ID               "memo"
#define CREATE_SHARED_MEMO_ID	     "shared-memo"
#define CREATE_MEMO_LIST_ID          "memo-list"

#define WEB_BASE_URI "webcal://"
#define PERSONAL_RELATIVE_URI "system"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

typedef struct _MemosComponentView
{
	ESourceList *source_list;

	GSList *source_selection;

	EMemos *memos;
	ETable *table;
	ETableModel *model;

	GtkWidget *source_selector;

	GList *notifications;

} MemosComponentView;

struct _MemosComponentPrivate {

	ESourceList *source_list;
	GSList *source_selection;

	ECal *create_ecal;

	GList *notifications;
};

#define d(x)

/* Utility functions.  */
/* FIXME Some of these are duplicated from calendar-component.c */
static gboolean
is_in_selection (GSList *selection, ESource *source)
{
	GSList *l;

	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;

		if (!strcmp (e_source_peek_uid (selected_source), e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static gboolean
is_in_uids (GSList *uids, ESource *source)
{
	GSList *l;

	for (l = uids; l; l = l->next) {
		const gchar *uid = l->data;

		if (!strcmp (uid, e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static void
source_selection_changed_cb (ESourceSelector *selector, MemosComponentView *component_view)
{
	GSList *selection, *l, *uids_selected = NULL;

	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (component_view->source_selector));

	for (l = component_view->source_selection; l; l = l->next) {
		ESource *old_selected_source = l->data;

		if (!is_in_selection (selection, old_selected_source))
			e_memos_remove_memo_source (component_view->memos, old_selected_source);
	}

	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;

		e_memos_add_memo_source (component_view->memos, selected_source);
		uids_selected = g_slist_append (uids_selected, (gchar *)e_source_peek_uid (selected_source));
	}

	e_source_selector_free_selection (component_view->source_selection);
	component_view->source_selection = selection;

	/* Save the selection for next time we start up */
	calendar_config_set_memos_selected (uids_selected);
	g_slist_free (uids_selected);
}

/* Evolution::Component CORBA methods */

static gboolean
selector_tree_data_dropped (ESourceSelector *selector,
                            GtkSelectionData *data,
                            ESource *destination,
                            GdkDragAction action,
                            guint info,
                            MemosComponent *component)
{
	gboolean success = FALSE;
	icalcomponent *icalcomp = NULL;
	ECal *client = NULL;
	GSList *components, *p;

	client = auth_new_cal_from_source (
		destination, E_CAL_SOURCE_TYPE_JOURNAL);

	if (!client || !e_cal_open (client, TRUE, NULL))
		goto  finish;

	components = cal_comp_selection_get_string_list (data);
	success = components != NULL;
	for (p = components; p && success; p = p->next) {
		gchar *comp_str; /* do not free this! */

		/* p->data is "source_uid\ncomponent_string" */
		comp_str = strchr (p->data, '\n');
		if (!comp_str)
			continue;

		comp_str [0] = 0;
		comp_str++;
		icalcomp = icalparser_parse_string (comp_str);

		if (!icalcomp)
			continue;

		success = cal_comp_process_source_list_drop (client, icalcomp, action, p->data, component->priv->source_list);
		icalcomponent_free (icalcomp);
	}

	g_slist_foreach (components, (GFunc)g_free, NULL);
	g_slist_free (components);

 finish:
	if (client)
		g_object_unref (client);

	return success;
}

static void
config_create_ecal_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	MemosComponent *component = data;
	MemosComponentPrivate *priv;

	priv = component->priv;

	g_object_unref (priv->create_ecal);
	priv->create_ecal = NULL;

	priv->notifications = g_list_remove (priv->notifications, GUINT_TO_POINTER (id));
}

static ECal *
setup_create_ecal (MemosComponent *component, MemosComponentView *component_view)
{
	MemosComponentPrivate *priv;
	ESource *source = NULL;
	gchar *uid;
	guint not;

	priv = component->priv;

	if (component_view) {
		ECal *default_ecal;

		default_ecal = e_memos_get_default_client (component_view->memos);
		if (default_ecal)
			return default_ecal;
	}

	if (priv->create_ecal)
		return priv->create_ecal;

	/* Get the current primary calendar, or try to set one if it doesn't already exist */
	uid = calendar_config_get_primary_memos ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);

		priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
	}

	if (!priv->create_ecal) {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (priv->source_list);
		if (source)
			priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
	}

	if (priv->create_ecal) {

		if (!e_cal_open (priv->create_ecal, FALSE, NULL)) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open the memo list '%s' for creating events and meetings"),
							   e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return NULL;
		}

	} else {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 "%s", _("There is no calendar available for creating memos"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return NULL;
	}

	/* Handle the fact it may change on us */
	not = calendar_config_add_notification_primary_memos (config_create_ecal_changed_cb,
							      component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Save the primary source for use elsewhere */
	calendar_config_set_primary_memos (e_source_peek_uid (source));

	return priv->create_ecal ;
}

/* Ensures the calendar is selected */
static void
object_created_cb (CompEditor *ce, EMemoTable *memo_table)
{
	g_return_if_fail (memo_table != NULL);

	memo_table->user_created_cal = comp_editor_get_client (ce);
	g_signal_emit_by_name (memo_table, "user_created");
	memo_table->user_created_cal = NULL;
}

static gboolean
create_new_memo (MemosComponent *memo_component, gboolean is_assigned, MemosComponentView *component_view)
{
	ECal *ecal;
	ECalComponent *comp;
	CompEditor *editor;
	CompEditorFlags flags = 0;

	ecal = setup_create_ecal (memo_component, component_view);
	if (!ecal)
		return FALSE;

	flags |= COMP_EDITOR_NEW_ITEM;
	if (is_assigned) {
		flags |= COMP_EDITOR_IS_SHARED;
		flags |= COMP_EDITOR_USER_ORG;
	}

	editor = memo_editor_new (ecal, flags);
	comp = cal_comp_memo_new_with_defaults (ecal);

	if (component_view)
		g_signal_connect (editor, "object_created", G_CALLBACK (object_created_cb), e_memos_get_calendar_table (component_view->memos));

	comp_editor_edit_comp (editor, comp);
	gtk_window_present (GTK_WINDOW (editor));

	e_comp_editor_registry_add (comp_editor_registry, editor, TRUE);

	return TRUE;
}

static MemosComponentView *
create_component_view (MemosComponent *memos_component)
{
	MemosComponentPrivate *priv;
	MemosComponentView *component_view;
	GtkWidget *statusbar_widget;

	priv = memos_component->priv;

	/* Create the calendar component view */
	component_view = g_new0 (MemosComponentView, 1);

	/* Create sidebar selector */
	g_signal_connect (component_view->source_selector, "drag-data-received",
			  G_CALLBACK (selector_tree_drag_data_received), memos_component);

	component_view->memos = (EMemos *) bonobo_control_get_widget (component_view->view_control);
	component_view->table = e_memo_table_get_table (e_memos_get_calendar_table (component_view->memos));
	component_view->model = E_TABLE_MODEL (e_memo_table_get_model (e_memos_get_calendar_table (component_view->memos)));

	/* connect after setting the initial selections, or we'll get unwanted calls
	   to calendar_control_sensitize_calendar_commands */
	g_signal_connect (component_view->source_selector, "selection_changed",
			  G_CALLBACK (source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "primary_selection_changed",
			  G_CALLBACK (primary_source_selection_changed_cb), component_view);

	return component_view;
}

static void
destroy_component_view (MemosComponentView *component_view)
{
	GList *l;

	if (component_view->source_list)
		g_object_unref (component_view->source_list);

	if (component_view->source_selection)
		e_source_selector_free_selection (component_view->source_selection);

	for (l = component_view->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (component_view->notifications);

	g_free (component_view);
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	MemosComponent *memos_component = MEMOS_COMPONENT (object);
	MemosComponentPrivate *priv = memos_component->priv;
	GList *l;

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}
	if (priv->source_selection != NULL) {
		e_source_selector_free_selection (priv->source_selection);
		priv->source_selection = NULL;
	}

	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}

	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (priv->notifications);
	priv->notifications = NULL;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}
