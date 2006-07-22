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

#include <evolution-shell-component-utils.h>
#include <e-util/e-util-private.h>
#include "memo-page.h"
#include "cancel-comp.h"
#include "../calendar-config.h"
#include "memo-editor.h"

struct _MemoEditorPrivate {
	MemoPage *memo_page;
	
	gboolean updating;	
};

static void memo_editor_set_e_cal (CompEditor *editor, ECal *client);
static void memo_editor_edit_comp (CompEditor *editor, ECalComponent *comp);
static gboolean memo_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method);
static void memo_editor_finalize (GObject *object);

G_DEFINE_TYPE (MemoEditor, memo_editor, TYPE_COMP_EDITOR)



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
}

static void
client_changed_cb (CompEditorPage *page, ECal *client, gpointer user_data)
{
/*	set_menu_sens (MEMO_EDITOR (user_data)); */
}

static void
menu_show_categories_cb (BonoboUIComponent         	*component,
		         const char                  	*path,
		         Bonobo_UIComponent_EventType 	type,
		         const char                  	*state,
		         gpointer                     	user_data)
{
	MemoEditor *me = (MemoEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	memo_page_set_show_categories (me->priv->memo_page, atoi(state));	
	calendar_config_set_show_categories (atoi(state));
}

static void
menu_class_public_cb (BonoboUIComponent           	*ui_component,
		      const char                  	*path,
		      Bonobo_UIComponent_EventType 	type,
		      const char                  	*state,
		      gpointer			  	user_data)
{
	MemoEditor *me = (MemoEditor *) user_data;

	if (state[0] == '0')
		return;

	comp_editor_page_notify_changed (COMP_EDITOR_PAGE (me->priv->memo_page));
	memo_page_set_classification (me->priv->memo_page, E_CAL_COMPONENT_CLASS_PUBLIC);
}

static void
menu_class_private_cb (BonoboUIComponent          	*ui_component,
		       const char                  	*path,
		       Bonobo_UIComponent_EventType 	type,
		       const char                  	*state,
		       gpointer			  	user_data)
{
	MemoEditor *me = (MemoEditor *) user_data;
	if (state[0] == '0')
		return;
	
	comp_editor_page_notify_changed (COMP_EDITOR_PAGE (me->priv->memo_page));
	memo_page_set_classification (me->priv->memo_page, E_CAL_COMPONENT_CLASS_PRIVATE);
}

static void
menu_class_confidential_cb (BonoboUIComponent           	*ui_component,
		     	    const char                  	*path,
		     	    Bonobo_UIComponent_EventType 	type,
		     	    const char                  	*state,
		     	    gpointer				user_data)
{
	MemoEditor *me = (MemoEditor *) user_data;
	if (state[0] == '0')
		return;

	comp_editor_page_notify_changed (COMP_EDITOR_PAGE (me->priv->memo_page));
	memo_page_set_classification (me->priv->memo_page, E_CAL_COMPONENT_CLASS_CONFIDENTIAL);
}

/* Object initialization function for the memo editor */
static void
memo_editor_init (MemoEditor *me)
{
	MemoEditorPrivate *priv;
	CompEditor *editor = COMP_EDITOR(me);
	gboolean status;
	char *xmlfile;
	
	priv = g_new0 (MemoEditorPrivate, 1);
	me->priv = priv;

	priv->updating = FALSE;	

	bonobo_ui_component_freeze (editor->uic, NULL);

	xmlfile = g_build_filename (EVOLUTION_UIDIR, "evolution-memo-editor.xml", NULL);
	bonobo_ui_util_set_ui (editor->uic, PREFIX,
			       xmlfile,
			       "evolution-memo-editor", NULL);
	g_free (xmlfile);
	
	status = calendar_config_get_show_categories ();
	bonobo_ui_component_set_prop (
		editor->uic, "/commands/ViewCategories",
		"state", status ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		editor->uic, "ViewCategories",
		menu_show_categories_cb, editor);

	bonobo_ui_component_set_prop (
		editor->uic, "/commands/ActionClassPublic",
		"state", "1", NULL);
	bonobo_ui_component_add_listener (
		editor->uic, "ActionClassPublic",
		menu_class_public_cb, editor);
	bonobo_ui_component_add_listener (
		editor->uic, "ActionClassPrivate",
		menu_class_private_cb, editor);
	bonobo_ui_component_add_listener (
		editor->uic, "ActionClassConfidential",
		menu_class_confidential_cb, editor);

	bonobo_ui_component_thaw (editor->uic, NULL);	

	/* TODO add help stuff */
/*	comp_editor_set_help_section (COMP_EDITOR (me), "usage-calendar-memo"); */
}

MemoEditor *
memo_editor_construct (MemoEditor *me, ECal *client)
{
	MemoEditorPrivate *priv;
	CompEditor *editor = COMP_EDITOR (me);
	gboolean read_only = FALSE;
	guint32 flags = comp_editor_get_flags (editor);
	
	priv = me->priv;

	priv->memo_page = memo_page_new (editor->uic, flags);
	g_object_ref (priv->memo_page);
	gtk_object_sink (GTK_OBJECT (priv->memo_page));
	comp_editor_append_page (COMP_EDITOR (me), 
				 COMP_EDITOR_PAGE (priv->memo_page),
				 _("Memo"), TRUE);
	g_signal_connect (G_OBJECT (priv->memo_page), "client_changed",
			  G_CALLBACK (client_changed_cb), me);

	if (!e_cal_is_read_only (client, &read_only, NULL))
		read_only = TRUE;
	
	bonobo_ui_component_set_prop (editor->uic, "/Toolbar/ecal3", "hidden", "1", NULL);
	comp_editor_set_e_cal (COMP_EDITOR (me), client);
	
		

	init_widgets (me);

	return me;
}

static void
memo_editor_set_e_cal (CompEditor *editor, ECal *client)
{
	if (COMP_EDITOR_CLASS (memo_editor_parent_class)->set_e_cal)
		COMP_EDITOR_CLASS (memo_editor_parent_class)->set_e_cal (editor, client);
}

static void
memo_editor_edit_comp (CompEditor *editor, ECalComponent *comp)
{
	CompEditorFlags flags = comp_editor_get_flags (editor);
	ECal *client = comp_editor_get_e_cal (editor);
	
	if (flags & COMP_EDITOR_IS_SHARED)
		comp_editor_set_needs_send (editor, itip_organizer_is_user (comp, client));
	
	if (COMP_EDITOR_CLASS (memo_editor_parent_class)->edit_comp)
		COMP_EDITOR_CLASS (memo_editor_parent_class)->edit_comp (editor, comp);
}

static gboolean
memo_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method)
{
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
	ECalComponent *comp;

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
memo_editor_new (ECal *client, CompEditorFlags flags)
{
	MemoEditor *me;

	me = g_object_new (TYPE_MEMO_EDITOR, NULL);
	comp_editor_set_flags (COMP_EDITOR (me), flags);
	return memo_editor_construct (me, client);
}

