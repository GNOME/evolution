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
 *		Yang Wu <yang.wu@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "ea-jump-button.h"
#include "ea-calendar-helpers.h"
#include "ea-week-view.h"
#include "e-week-view.h"
#include <libgnomecanvas/libgnomecanvas.h>
#include <glib/gi18n.h>

static void ea_jump_button_class_init (EaJumpButtonClass *klass);

static const gchar * ea_jump_button_get_name (AtkObject *accessible);
static const gchar * ea_jump_button_get_description (AtkObject *accessible);

/* action interface */
static void                  atk_action_interface_init  (AtkActionIface *iface);
static gboolean              jump_button_do_action      (AtkAction      *action,
                                                         gint           i);
static gint                  jump_button_get_n_actions  (AtkAction      *action);
static const gchar * jump_button_get_keybinding (AtkAction      *action,
                                                         gint           i);

static gpointer parent_class = NULL;

GType
ea_jump_button_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaJumpButtonClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ea_jump_button_class_init,
			(GClassFinalizeFunc) NULL,
			/* class_data */ NULL,
			sizeof (EaJumpButton),
			/* n_preallocs */ 0,
			(GInstanceInitFunc) NULL,
			/* value_table */ NULL
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (atk object for
		 * GNOME_TYPE_CANVAS_ITEM, in this case)
		 */

		factory = atk_registry_get_factory (
			atk_get_default_registry (), GNOME_TYPE_CANVAS_ITEM);
		derived_atk_type =
			atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		/* we inherit the component and other
		 * interfaces from GNOME_TYPE_CANVAS_ITEM */
		type = g_type_register_static (
			derived_atk_type, "EaJumpButton", &tinfo, 0);

		g_type_add_interface_static (
			type, ATK_TYPE_ACTION, &atk_action_info);
	}

	return type;
}

static void
ea_jump_button_class_init (EaJumpButtonClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_jump_button_get_name;
	class->get_description = ea_jump_button_get_description;
}

AtkObject *
ea_jump_button_new (GObject *obj)
{
	AtkObject *atk_obj = NULL;
	GObject *target_obj;

	g_return_val_if_fail (GNOME_IS_CANVAS_ITEM (obj), NULL);

	target_obj = obj;
	atk_obj = g_object_get_data (target_obj, "accessible-object");

	if (!atk_obj) {
		static AtkRole event_role = ATK_ROLE_INVALID;
		atk_obj = ATK_OBJECT (
			g_object_new (EA_TYPE_JUMP_BUTTON, NULL));
		atk_object_initialize (atk_obj, target_obj);
		if (event_role == ATK_ROLE_INVALID)
			event_role = atk_role_register ("Jump Button");
		atk_obj->role = event_role;
	}

	/* The registered factory for GNOME_TYPE_CANVAS_ITEM cannot create
	 * an EaJumpbutton, we should save the EaJumpbutton object in it. */
	g_object_set_data (obj, "accessible-object", atk_obj);

	return atk_obj;
}

static const gchar *
ea_jump_button_get_name (AtkObject *accessible)
{
	g_return_val_if_fail (EA_IS_JUMP_BUTTON (accessible), NULL);

	if (accessible->name)
		return accessible->name;
	return _("Jump button");
}

static const gchar *
ea_jump_button_get_description (AtkObject *accessible)
{
	if (accessible->description)
		return accessible->description;

	return _("Click here, you can find more events.");
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->do_action = jump_button_do_action;
  iface->get_n_actions = jump_button_get_n_actions;
  iface->get_keybinding = jump_button_get_keybinding;
}

static gboolean
jump_button_do_action (AtkAction *action,
                       gint i)
{
  gboolean return_value = TRUE;
  AtkGObjectAccessible *atk_gobj;
  GObject *g_obj;
  GnomeCanvasItem *item;
  ECalendarView *cal_view;
  EWeekView *week_view;

  atk_gobj = ATK_GOBJECT_ACCESSIBLE (action);
  g_obj = atk_gobject_accessible_get_object (atk_gobj);
  if (!g_obj)
	  return FALSE;

  item = GNOME_CANVAS_ITEM (g_obj);
  cal_view = ea_calendar_helpers_get_cal_view_from (GNOME_CANVAS_ITEM (item));
  week_view = E_WEEK_VIEW (cal_view);

  switch (i)
    {
    case 0:
	    e_week_view_jump_to_button_item (week_view, GNOME_CANVAS_ITEM (item));
	    break;
    default:
	    return_value = FALSE;
      break;
    }
  return return_value;
}

static gint
jump_button_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar *
jump_button_get_keybinding (AtkAction *action,
                            gint i)
{
  const gchar *return_value = NULL;

  switch (i)
    {
    case 0:
      {
	      return_value = "space or enter";
	      break;
      }
    default:
      break;
    }
  return return_value;
}
