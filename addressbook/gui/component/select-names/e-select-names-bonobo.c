/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-select-names-bonobo.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-select-names-bonobo.h"

#include <bonobo-activation/bonobo-activation-activate.h>

#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-event-source.h>
#include <bonobo/bonobo-ui-util.h>

#include <gal/util/e-util.h>
#include <gal/e-text/e-entry.h>
#include <gal/util/e-text-event-processor.h>

#include "Evolution-Addressbook-SelectNames.h"

#include "e-select-names-manager.h"
#include "e-select-names-model.h"
#include "e-select-names-text-model.h"
#include "e-select-names-completion.h"

#include <string.h>


#define PARENT_TYPE BONOBO_TYPE_OBJECT
static BonoboObjectClass *parent_class = NULL;

struct _ESelectNamesBonoboPrivate {
	ESelectNamesManager *manager;
	BonoboEventSource *event_source;
};

enum _EntryPropertyID {
	ENTRY_PROPERTY_ID_TEXT,
	ENTRY_PROPERTY_ID_ADDRESSES,
	ENTRY_PROPERTY_ID_DESTINATIONS,
	ENTRY_PROPERTY_ID_ALLOW_CONTACT_LISTS,
	ENTRY_PROPERTY_ID_ENTRY_CHANGED
};
typedef enum _EntryPropertyID EntryPropertyID;


/* PropertyBag implementation for the entry widgets.  */

static void
entry_get_property_fn (BonoboPropertyBag *bag,
		       BonoboArg *arg,
		       unsigned int arg_id,
		       CORBA_Environment *ev,
		       void *user_data)
{
	GtkWidget *w;

	w = GTK_WIDGET (user_data);

	switch (arg_id) {
	case ENTRY_PROPERTY_ID_TEXT:
		{
			ETextModel *text_model;
			text_model = E_TEXT_MODEL (g_object_get_data (G_OBJECT (w), "select_names_text_model"));
			g_assert (text_model != NULL);
			
			BONOBO_ARG_SET_STRING (arg, e_text_model_get_text (text_model));
		break;
		}

	case ENTRY_PROPERTY_ID_ADDRESSES:
		{
			ESelectNamesModel *model;
			char *text;

			model = E_SELECT_NAMES_MODEL (g_object_get_data (G_OBJECT (w), "select_names_model"));
			g_assert (model != NULL);

			text = e_select_names_model_get_address_text (model, ", ");
			BONOBO_ARG_SET_STRING (arg, text);
			g_free (text);
		}
		break;

	case ENTRY_PROPERTY_ID_DESTINATIONS:
		{
			ESelectNamesModel *model;
			char *text;

			model = E_SELECT_NAMES_MODEL (g_object_get_data (G_OBJECT (w), "select_names_model"));
			g_assert (model != NULL);

			text = e_select_names_model_export_destinationv (model);
			BONOBO_ARG_SET_STRING (arg, text);
			g_free (text);
		}
		break;

	case ENTRY_PROPERTY_ID_ALLOW_CONTACT_LISTS:
		{
			ESelectNamesCompletion *comp;
			comp = E_SELECT_NAMES_COMPLETION (g_object_get_data (G_OBJECT (w), "completion_handler"));
			g_assert (comp != NULL);

			BONOBO_ARG_SET_BOOLEAN (arg, e_select_names_completion_get_match_contact_lists (comp));
			break;
		}

	case ENTRY_PROPERTY_ID_ENTRY_CHANGED:
		/* This is a read-only property. */
		g_assert_not_reached ();
		break;

	default:
		break;
	}
}

static void
entry_set_property_fn (BonoboPropertyBag *bag,
		       const BonoboArg *arg,
		       guint arg_id,
		       CORBA_Environment *ev,
		       gpointer user_data)
{
	GtkWidget *w;

	w = GTK_WIDGET (user_data);

	switch (arg_id) {

	case ENTRY_PROPERTY_ID_TEXT:
	case ENTRY_PROPERTY_ID_ADDRESSES:
		{
			ESelectNamesModel *model;
			model = E_SELECT_NAMES_MODEL (g_object_get_data (G_OBJECT (w), "select_names_model"));
			g_assert (model != NULL);
			
			e_entry_set_text (E_ENTRY (w), BONOBO_ARG_GET_STRING (arg));
			e_select_names_model_load_contacts (model);
			break;
		}

	case ENTRY_PROPERTY_ID_DESTINATIONS:
		{
			ESelectNamesModel *model;
			model = E_SELECT_NAMES_MODEL (g_object_get_data (G_OBJECT (w), "select_names_model"));
			g_assert (model != NULL);

			e_select_names_model_import_destinationv (model, BONOBO_ARG_GET_STRING (arg));
			e_select_names_model_load_contacts (model);
			break;
		}

	case ENTRY_PROPERTY_ID_ALLOW_CONTACT_LISTS:
		{
			ESelectNamesCompletion *comp;
			comp = E_SELECT_NAMES_COMPLETION (g_object_get_data (G_OBJECT (w), "completion_handler"));
			g_assert (comp != NULL);

			e_select_names_completion_set_match_contact_lists (comp, BONOBO_ARG_GET_BOOLEAN (arg));
			break;
		}
		
	case ENTRY_PROPERTY_ID_ENTRY_CHANGED:
		g_object_set_data (G_OBJECT (w), "entry_property_id_changed", GUINT_TO_POINTER (1));
		break;

	default:
		break;
	}
}

static void
impl_SelectNames_add_section (PortableServer_Servant servant,
			      const CORBA_char *id,
			      const CORBA_char *title,
			      CORBA_Environment *ev)
{
	ESelectNamesBonobo *select_names;
	ESelectNamesBonoboPrivate *priv;

	select_names = E_SELECT_NAMES_BONOBO (bonobo_object (servant));
	priv = select_names->priv;

	e_select_names_manager_add_section (priv->manager, id, title);
}

static void
impl_SelectNames_add_section_with_limit (PortableServer_Servant servant,
					 const CORBA_char *id,
					 const CORBA_char *title,
					 CORBA_short limit,
					 CORBA_Environment *ev)
{
	ESelectNamesBonobo *select_names;
	ESelectNamesBonoboPrivate *priv;

	select_names = E_SELECT_NAMES_BONOBO (bonobo_object (servant));
	priv = select_names->priv;

	e_select_names_manager_add_section_with_limit (priv->manager, id, title, limit);
}

static void
entry_changed (GtkWidget *widget, BonoboControl *control)
{
	gboolean changed = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "entry_property_id_changed"));

	if (!changed)
		bonobo_control_set_property (control, NULL, "entry_changed", TC_CORBA_boolean, TRUE, NULL);
}

static void
manager_changed_cb (ESelectNamesManager *manager, const gchar *section_id, gint changed_working_copy, gpointer closure)
{
	ESelectNamesBonobo *select_names = E_SELECT_NAMES_BONOBO (closure);
	BonoboArg *arg;

	arg = bonobo_arg_new (BONOBO_ARG_STRING);
	BONOBO_ARG_SET_STRING (arg, section_id);

	bonobo_event_source_notify_listeners_full (select_names->priv->event_source,
						   "GNOME/Evolution",
						   "changed",
						   changed_working_copy ? "working_copy" : "model",
						   arg, NULL);

	bonobo_arg_release (arg);
}

static void
manager_ok_cb (ESelectNamesManager *manager, gpointer closure)
{
	ESelectNamesBonobo *select_names = E_SELECT_NAMES_BONOBO (closure);
	BonoboArg *arg;

	arg = bonobo_arg_new (BONOBO_ARG_NULL);

	bonobo_event_source_notify_listeners_full (select_names->priv->event_source,
						   "GNOME/Evolution",
						   "ok",
						   "dialog",
						   arg,
						   NULL);

	bonobo_arg_release (arg);
}

static void
copy_cb (BonoboUIComponent *ui, gpointer user_data, const char *command)
{
	EEntry *entry = E_ENTRY (user_data);

	e_text_copy_clipboard (entry->item);
}

static void
cut_cb (BonoboUIComponent *ui, gpointer user_data, const char *command)
{
	EEntry *entry = E_ENTRY (user_data);

	e_text_cut_clipboard (entry->item);
}

static void
paste_cb (BonoboUIComponent *ui, gpointer user_data, const char *command)
{
	EEntry *entry = E_ENTRY (user_data);

	e_text_paste_clipboard (entry->item);
}

static void
select_all_cb (BonoboUIComponent *ui, gpointer user_data, const char *command)
{
	EEntry *entry = E_ENTRY (user_data);

	e_text_select_all (entry->item);
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("EditCut", cut_cb),
	BONOBO_UI_VERB ("EditCopy", copy_cb),
	BONOBO_UI_VERB ("EditPaste", paste_cb),
	BONOBO_UI_VERB ("EditSelectAll", select_all_cb),
	BONOBO_UI_VERB_END
};

typedef struct {
	GtkWidget *widget;
	BonoboControl *control;
	Bonobo_UIContainer remote_ui_container;
	char *ui_xml_path;
	char *app_name;
	BonoboUIVerb *verbs;
	gpointer user_data;
} ControlUIClosure;

static void
free_closure (ControlUIClosure *closure,
	      GtkObject *where_object_was)
{
	bonobo_object_release_unref (closure->remote_ui_container, NULL);
	g_free (closure->ui_xml_path);
	g_free (closure->app_name);
	g_free (closure);
}

static void
merge_menu_items (BonoboControl *control, BonoboUIComponent *uic, ControlUIClosure *closure)
{
	if (closure->remote_ui_container) {
		bonobo_ui_component_set_container (uic, closure->remote_ui_container, NULL);

		bonobo_ui_component_add_verb_list_with_data (uic, closure->verbs, closure->user_data);

		bonobo_ui_component_freeze (uic, NULL);

		bonobo_ui_util_set_ui (uic, PREFIX,
				       closure->ui_xml_path,
				       closure->app_name, NULL);

		bonobo_ui_component_thaw (uic, NULL);
	}
}

static void
unmerge_menu_items (BonoboControl *control, BonoboUIComponent *uic, ControlUIClosure *closure)
{
	bonobo_ui_component_unset_container (uic, NULL);
}

static void
control_set_frame_cb (BonoboControl *control,
		      ControlUIClosure *closure)
{
	Bonobo_ControlFrame frame = bonobo_control_get_control_frame (control,
								      NULL);
	if (!frame)
		return;
	closure->remote_ui_container = bonobo_control_get_remote_ui_container (control, NULL);
}

static void
control_activate_cb (BonoboControl *control,
		     gboolean activate, 
		     ControlUIClosure *closure)
{
	if (activate)
		gtk_widget_grab_focus (closure->widget); /* the ECanvas */
}

static gboolean
widget_focus_cb (GtkWidget *w, GdkEventFocus *focus, ControlUIClosure *closure)
{
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (closure->control);

	if (GTK_WIDGET_HAS_FOCUS (w)) {
		merge_menu_items (closure->control, uic, closure);
	} else {
		unmerge_menu_items (closure->control, uic, closure);
	}

	return FALSE;
}

static void
e_bonobo_control_automerge_ui (GtkWidget *w,
			       BonoboControl *control,
			       const char *ui_xml_path,
			       const char *app_name,
			       BonoboUIVerb *verbs,
			       gpointer data)
{
	ControlUIClosure *closure;

	g_return_if_fail (GTK_IS_WIDGET (w));
	g_return_if_fail (BONOBO_IS_CONTROL (control));
	g_return_if_fail (ui_xml_path != NULL);
	g_return_if_fail (app_name != NULL);
	g_return_if_fail (verbs != NULL);
	
	closure = g_new (ControlUIClosure, 1);

	closure->widget = w;
	closure->control = control;
	closure->ui_xml_path = g_strdup (ui_xml_path);
	closure->app_name = g_strdup (app_name);
	closure->verbs = verbs;
	closure->user_data = data;

	g_signal_connect (w, "focus_in_event",
			  G_CALLBACK (widget_focus_cb), closure);
	g_signal_connect (w, "focus_out_event",
			  G_CALLBACK (widget_focus_cb), closure);
	g_signal_connect (control, "activate",
			  G_CALLBACK (control_activate_cb), closure);
	g_signal_connect (control, "set_frame",
			  G_CALLBACK (control_set_frame_cb), closure);

	g_object_weak_ref (G_OBJECT (control), (GWeakNotify)free_closure, closure);
}

static Bonobo_Control
impl_SelectNames_get_entry_for_section (PortableServer_Servant servant,
					const CORBA_char *section_id,
					CORBA_Environment *ev)
{
	ESelectNamesBonobo *select_names;
	ESelectNamesBonoboPrivate *priv;
	GtkWidget *entry_widget;
	BonoboControl *control;
	BonoboPropertyBag *property_bag;

	select_names = E_SELECT_NAMES_BONOBO (bonobo_object (servant));
	priv = select_names->priv;

	entry_widget = e_select_names_manager_create_entry (priv->manager, section_id);
	gtk_widget_show (entry_widget);
	
	if (entry_widget == NULL) {
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Addressbook_SelectNames_SectionNotFound,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	control = bonobo_control_new (entry_widget);

	property_bag = bonobo_property_bag_new (entry_get_property_fn, entry_set_property_fn, entry_widget);
	bonobo_property_bag_add (property_bag, "text", ENTRY_PROPERTY_ID_TEXT,
				 BONOBO_ARG_STRING, NULL, NULL,
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (property_bag, "addresses", ENTRY_PROPERTY_ID_ADDRESSES,
				 BONOBO_ARG_STRING, NULL, NULL,
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (property_bag, "destinations", ENTRY_PROPERTY_ID_DESTINATIONS,
				 BONOBO_ARG_STRING, NULL, NULL,
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (property_bag, "allow_contact_lists", ENTRY_PROPERTY_ID_ALLOW_CONTACT_LISTS,
				 BONOBO_ARG_BOOLEAN, NULL, NULL,
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (property_bag, "entry_changed", ENTRY_PROPERTY_ID_ENTRY_CHANGED,
				 BONOBO_ARG_BOOLEAN, NULL, NULL,
				 BONOBO_PROPERTY_WRITEABLE);

	bonobo_control_set_properties (control, bonobo_object_corba_objref (BONOBO_OBJECT (property_bag)), NULL);
	bonobo_object_unref (BONOBO_OBJECT (property_bag));

	g_signal_connect (entry_widget, "changed", G_CALLBACK (entry_changed), control);

	e_bonobo_control_automerge_ui (GTK_WIDGET (E_ENTRY (entry_widget)->canvas),
				       control,
				       EVOLUTION_UIDIR "/evolution-composer-entries.xml",
				       "evolution-addressbook",
				       verbs, entry_widget);

	return CORBA_Object_duplicate (bonobo_object_corba_objref (BONOBO_OBJECT (control)), ev);
}

static void
impl_SelectNames_activate_dialog (PortableServer_Servant servant,
				  const CORBA_char *section_id,
				  CORBA_Environment *ev)
{
	ESelectNamesBonobo *select_names;
	ESelectNamesBonoboPrivate *priv;

	select_names = E_SELECT_NAMES_BONOBO (bonobo_object (servant));
	priv = select_names->priv;

	e_select_names_manager_activate_dialog (priv->manager, section_id);
}


/* GtkObject methods.  */

static void
impl_dispose (GObject *object)
{
	ESelectNamesBonobo *select_names;
	ESelectNamesBonoboPrivate *priv;

	select_names = E_SELECT_NAMES_BONOBO (object);
	priv = select_names->priv;

	if (priv) {
		if (priv->manager->names) {
			gtk_widget_destroy (GTK_WIDGET (priv->manager->names));
			priv->manager->names = NULL;
		}

		g_object_unref (priv->manager);

		g_free (priv);
		select_names->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
e_select_names_bonobo_class_init (ESelectNamesBonoboClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_Addressbook_SelectNames__epv *epv;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = impl_dispose;

	epv = &klass->epv;
	epv->addSection          = impl_SelectNames_add_section;
	epv->addSectionWithLimit = impl_SelectNames_add_section_with_limit;
	epv->getEntryBySection   = impl_SelectNames_get_entry_for_section;
	epv->activateDialog      = impl_SelectNames_activate_dialog;
}

static void
e_select_names_bonobo_init (ESelectNamesBonobo *select_names)
{
	ESelectNamesBonoboPrivate *priv;

	priv = g_new (ESelectNamesBonoboPrivate, 1);

	priv->manager = e_select_names_manager_new ();
	priv->event_source = NULL;

	g_signal_connect (priv->manager,
			  "changed",
			  G_CALLBACK (manager_changed_cb),
			  select_names);

	g_signal_connect (priv->manager,
			  "ok",
			  G_CALLBACK (manager_ok_cb),
			  select_names);
	
	select_names->priv = priv;
}


static void
e_select_names_bonobo_construct (ESelectNamesBonobo *select_names)
{
	g_return_if_fail (select_names != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_BONOBO (select_names));

	g_assert (select_names->priv->event_source == NULL);
	select_names->priv->event_source = bonobo_event_source_new ();
	bonobo_object_add_interface (BONOBO_OBJECT (select_names), BONOBO_OBJECT (select_names->priv->event_source));
}

ESelectNamesBonobo *
e_select_names_bonobo_new (void)
{
	ESelectNamesBonobo *select_names;

	select_names = g_object_new (E_TYPE_SELECT_NAMES_BONOBO, NULL);

	e_select_names_bonobo_construct (select_names);

	return select_names;
}


BONOBO_TYPE_FUNC_FULL (
		       ESelectNamesBonobo,
		       GNOME_Evolution_Addressbook_SelectNames,
		       PARENT_TYPE,
		       e_select_names_bonobo);
