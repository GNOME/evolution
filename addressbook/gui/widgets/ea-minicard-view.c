/*
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
 *		Leon Zhang < leon.zhang@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "ea-minicard.h"
#include "ea-minicard-view.h"
#include "eab-gui-util.h"
#include "e-addressbook-view.h"

static const gchar * action_name[] = {
	N_("New Contact"),
	N_("New Contact List")
};

static const gchar * ea_minicard_view_get_name (AtkObject *accessible);
static const gchar * ea_minicard_view_get_description (AtkObject *accessible);

static void ea_minicard_view_class_init (EaMinicardViewClass *klass);

static gint ea_minicard_view_get_n_children (AtkObject *obj);
static AtkObject *ea_minicard_view_ref_child (AtkObject *obj, gint i);

static AtkStateSet *ea_minicard_view_ref_state_set (AtkObject *obj);

static void atk_selection_interface_init (AtkSelectionIface *iface);
static gboolean selection_interface_add_selection (AtkSelection *selection,
						   gint i);
static gboolean selection_interface_clear_selection (AtkSelection *selection);
static AtkObject * selection_interface_ref_selection (AtkSelection *selection,
						     gint i);
static gint selection_interface_get_selection_count (AtkSelection *selection);
static gboolean selection_interface_is_child_selected (AtkSelection *selection,
						       gint i);

static void atk_action_interface_init (AtkActionIface *iface);
static gboolean atk_action_interface_do_action (AtkAction *iface, gint i);
static gint atk_action_interface_get_n_action (AtkAction *iface);
static const gchar *
		atk_action_interface_get_description
						(AtkAction *iface,
						 gint i);
static const gchar *
		atk_action_interface_get_name	(AtkAction *iface,
						 gint i);

static gpointer parent_class = NULL;

GType
ea_minicard_view_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaMinicardViewClass),
			(GBaseInitFunc) NULL,  /* base_init */
			(GBaseFinalizeFunc) NULL,  /* base_finalize */
			(GClassInitFunc) ea_minicard_view_class_init,
			(GClassFinalizeFunc) NULL, /* class_finalize */
			NULL,	   /* class_data */
			sizeof (EaMinicardView),
			0,	     /* n_preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		static const GInterfaceInfo atk_selection_info = {
			(GInterfaceInitFunc) atk_selection_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (GailWidget, in this case) */

		factory = atk_registry_get_factory (
			atk_get_default_registry (),
			GNOME_TYPE_CANVAS_GROUP);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (
			derived_atk_type,
			"EaMinicardView", &tinfo, 0);
		g_type_add_interface_static (
			type, ATK_TYPE_SELECTION,
			&atk_selection_info);
		g_type_add_interface_static (
			type, ATK_TYPE_ACTION,
			&atk_action_info);

	}

	return type;
}

static void
adapter_notify_client_cb (EAddressbookReflowAdapter *adapter,
			  GParamSpec *param,
			  AtkObject *accessible)
{
	atk_object_set_name (accessible, ea_minicard_view_get_name (accessible));
}

static void
ea_minicard_view_dispose (GObject *object)
{
	EMinicardView *card_view = NULL;
	GObject *gobj;

	gobj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (object));

	if (E_IS_MINICARD_VIEW (gobj))
		card_view = E_MINICARD_VIEW (gobj);

	if (card_view && card_view->adapter) {
		g_signal_handlers_disconnect_by_func (card_view->adapter, adapter_notify_client_cb, object);
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ea_minicard_view_class_init (EaMinicardViewClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_minicard_view_get_name;
	class->get_description = ea_minicard_view_get_description;
	class->ref_state_set = ea_minicard_view_ref_state_set;
	class->get_n_children = ea_minicard_view_get_n_children;
	class->ref_child = ea_minicard_view_ref_child;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = ea_minicard_view_dispose;
}

static const gchar *
ea_minicard_view_get_name (AtkObject *accessible)
{
	EReflow *reflow;
	gchar *string;
	EMinicardView *card_view;
	EBookClient *book_client = NULL;
	ESource *source;
	const gchar *display_name;

	g_return_val_if_fail (EA_IS_MINICARD_VIEW (accessible), NULL);

	reflow = E_REFLOW (atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (accessible)));

	if (!reflow)
		return NULL;

	/* Get the current name of minicard view*/
	card_view = E_MINICARD_VIEW (reflow);
	g_object_get (card_view->adapter, "client", &book_client, NULL);
	if (!book_client)
		return accessible->name;
	g_return_val_if_fail (E_IS_BOOK_CLIENT (book_client), NULL);
	source = e_client_get_source (E_CLIENT (book_client));
	display_name = e_source_get_display_name (source);
	if (display_name == NULL)
		display_name = "";

	string = g_strdup_printf (
		ngettext ("current address book folder %s has %d card",
		"current address book folder %s has %d cards",
		reflow->count), display_name, reflow->count);

	ATK_OBJECT_CLASS (parent_class)->set_name (accessible, string);
	g_free (string);
	g_object_unref (book_client);
	return accessible->name;
}

static const gchar *
ea_minicard_view_get_description (AtkObject *accessible)
{
	g_return_val_if_fail (EA_IS_MINICARD_VIEW (accessible), NULL);
	if (accessible->description)
		return accessible->description;

	return _("evolution address book");
}

AtkObject *
ea_minicard_view_new (GObject *obj)
{
	GObject *object;
	AtkObject *accessible;
	EMinicardView *card_view;

	g_return_val_if_fail (E_IS_MINICARD_VIEW (obj), NULL);
	object = g_object_new (EA_TYPE_MINICARD_VIEW, NULL);
	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, obj);
	accessible->role = ATK_ROLE_PANEL;

	card_view = E_MINICARD_VIEW (obj);
	if (card_view->adapter)
		g_signal_connect (card_view->adapter, "notify::client", G_CALLBACK (adapter_notify_client_cb), accessible);

	return accessible;
}

static gint
ea_minicard_view_get_n_children (AtkObject *accessible)
{
	EReflow *reflow;

	gint child_num = 0;

	g_return_val_if_fail (EA_IS_MINICARD_VIEW (accessible), -1);

	reflow = E_REFLOW (atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (accessible)));

	if (!reflow)
		return -1;

	child_num = reflow->count;

	return child_num;
}

static AtkStateSet *ea_minicard_view_ref_state_set (AtkObject *obj)
{
	AtkStateSet *state_set = NULL;
	GObject *gobj = NULL;

	state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (obj);
	if (!state_set)
		state_set = atk_state_set_new ();

	gobj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj));
	if (!gobj)
		return state_set;

	atk_state_set_add_state (state_set, ATK_STATE_ENABLED);
	atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);
	atk_state_set_add_state (state_set, ATK_STATE_SHOWING);

	return state_set;
}

static AtkObject *
ea_minicard_view_ref_child (AtkObject *accessible,
                            gint index)
{
	EReflow *reflow;
	gint child_num;
	AtkObject *atk_object = NULL;
	EMinicard *card = NULL;

	g_return_val_if_fail (EA_IS_MINICARD_VIEW (accessible), NULL);

	child_num = atk_object_get_n_accessible_children (accessible);
	if (child_num <= 0 || index < 0 || index >= child_num)
		return NULL;

	reflow = E_REFLOW (atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (accessible)));
	if (!reflow)
		return NULL;
	if (!reflow->items)
		return NULL;
		/* a minicard */
	if (index < child_num) {
		if (reflow->items[index] == NULL) {
			reflow->items[index] = e_reflow_model_incarnate (reflow->model, index, GNOME_CANVAS_GROUP (reflow));
			g_object_set (
				reflow->items[index],
				"width", (double) reflow->column_width,
				NULL);
		}
		card = E_MINICARD (reflow->items[index]);
		atk_object = atk_gobject_accessible_for_object (G_OBJECT (card));
	} else {
		return NULL;
	}

	g_object_ref (atk_object);
		return atk_object;
}

/* atkselection interface */

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->add_selection = selection_interface_add_selection;
	iface->clear_selection = selection_interface_clear_selection;
	iface->ref_selection = selection_interface_ref_selection;
	iface->get_selection_count = selection_interface_get_selection_count;
	iface->is_child_selected = selection_interface_is_child_selected;
}

static gboolean
selection_interface_add_selection (AtkSelection *selection,
                                   gint i)
{
	AtkGObjectAccessible *atk_gobj= NULL;
	EReflow *reflow = NULL;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (selection);
	reflow = E_REFLOW (atk_gobject_accessible_get_object (atk_gobj));

	if (!reflow)
		return FALSE;

	selection_interface_clear_selection (selection);
	e_selection_model_select_single_row (reflow->selection, i);

	return TRUE;
}

static gboolean
selection_interface_clear_selection (AtkSelection *selection)
{
	AtkGObjectAccessible *atk_gobj = NULL;
	EReflow *reflow = NULL;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (selection);
	reflow = E_REFLOW (atk_gobject_accessible_get_object (atk_gobj));

	if (!reflow)
		return FALSE;

	e_selection_model_clear (reflow->selection);

	return TRUE;
}

static AtkObject *
selection_interface_ref_selection (AtkSelection *selection,
                                   gint i)
{
	return ea_minicard_view_ref_child (ATK_OBJECT (selection), i);
}

static gint
selection_interface_get_selection_count (AtkSelection *selection)
{
	AtkGObjectAccessible *atk_gobj = NULL;
	EReflow *reflow = NULL;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (selection);
	reflow = E_REFLOW (atk_gobject_accessible_get_object (atk_gobj));

	if (!reflow)
		return FALSE;

	return e_selection_model_selected_count (reflow->selection);
}

static gboolean
selection_interface_is_child_selected (AtkSelection *selection,
                                       gint i)
{
	AtkGObjectAccessible *atk_gobj = NULL;
	EReflow *reflow = NULL;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (selection);
	reflow = E_REFLOW (atk_gobject_accessible_get_object (atk_gobj));

	if (!reflow)
		return FALSE;

	return e_selection_model_is_row_selected (reflow->selection, i);
}

static void atk_action_interface_init (AtkActionIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->do_action = atk_action_interface_do_action;
	iface->get_n_actions = atk_action_interface_get_n_action;
	iface->get_description = atk_action_interface_get_description;
	iface->get_name = atk_action_interface_get_name;
}

static gboolean
atk_action_interface_do_action (AtkAction *action,
                                gint i)
{
	gboolean return_value = TRUE;
	EMinicardView *card_view;

	AtkGObjectAccessible *atk_gobj= NULL;
	EReflow *reflow = NULL;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (action);
	reflow = E_REFLOW (atk_gobject_accessible_get_object (atk_gobj));

	if (reflow == NULL)
		return FALSE;

	card_view = E_MINICARD_VIEW (reflow);

	switch (i) {
		case 0:
		/* New Contact */
			e_minicard_view_create_contact (card_view);
			break;
		case 1:
		/* New Contact List */
			e_minicard_view_create_contact_list (card_view);
			break;
		default:
			return_value = FALSE;
			break;
	}

	return return_value;
}

static gint
atk_action_interface_get_n_action (AtkAction *iface)
{
	return G_N_ELEMENTS (action_name);
}

static const gchar *
atk_action_interface_get_description (AtkAction *iface,
                                      gint i)
{
	return atk_action_interface_get_name (iface, i);
}

static const gchar *
atk_action_interface_get_name (AtkAction *iface,
                               gint i)
{
	if (i >= G_N_ELEMENTS (action_name) || i < 0)
		return NULL;

	return action_name[i];
}

