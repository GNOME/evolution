/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Michel Zucchi <notzed@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __E_POPUP_H__
#define __E_POPUP_H__

#include <glib-object.h>
#include "e-util/e-msgport.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* This is an abstract popup menu management/merging class.

   To implement your own popup menu system, just create your own
   target types and implement the target free method. */

typedef struct _EPopup EPopup;
typedef struct _EPopupClass EPopupClass;

typedef struct _EPopupItem EPopupItem;
typedef struct _EPopupFactory EPopupFactory; /* anonymous type */
typedef struct _EPopupTarget EPopupTarget;

typedef void (*EPopupActivateFunc)(EPopup *ep, EPopupItem *item, void *data);
typedef void (*EPopupFactoryFunc)(EPopup *emp, void *data);
typedef void (*EPopupItemsFunc)(EPopup *ep, GSList *items, void *data);

/**
 * enum _e_popup_t - Popup item type enumeration.
 * @E_POPUP_ITEM: A simple menu item.
 * @E_POPUP_TOGGLE: A toggle menu item.
 * @E_POPUP_RADIO: A radio menu item.  Note that the radio group is
 * global for the entire (sub) menu.  i.e. submenu's must be used to
 * separate radio button menu items.
 * @E_POPUP_IMAGE: A &GtkImage menu item.  In this case the @image
 * field of &struct _EPopupItem points to the &GtkImage directly.
 * @E_POPUP_SUBMENU: A sub-menu header.  It is up to the application
 * to define the @path properly so that the submenu comes before the
 * submenu items.
 * @E_POPUP_BAR: A menu separator bar.
 * @E_POPUP_TYPE_MASK: Mask used to separate item type from option bits.
 * @E_POPUP_ACTIVE: An option bit to signify that the radio button or
 * toggle button is active.
 */
enum _e_popup_t {
	E_POPUP_ITEM = 0,
	E_POPUP_TOGGLE,
	E_POPUP_RADIO,
	E_POPUP_IMAGE,
	E_POPUP_SUBMENU,
	E_POPUP_BAR,
	E_POPUP_TYPE_MASK = 0xffff,
	E_POPUP_ACTIVE = 0x10000,
};

/* FIXME: activate passes back no context data apart from that provided.
   FIXME: It should pass the target at the least.  The menu widget is
   useless */

/**
 * struct _EPopupItem - A popup menu item definition.
 * @type: The type of the popup.  See the &enum _epopup_t definition
 * for possible values.
 * @path: An absolute path, which when sorted using a simple ASCII
 * sort, will put the menu item in the right place in the menu
 * heirarchy.  '/' is used to separate menus from submenu items.
 * @label: The text of the menyu item.
 * @activate: A function conforming to &EPopupActivateFunc which will
 * be called when the menu item is activated.
 * @user_data: Extra per-item user-data available to the
 * application.  This is not passed to the @data field of @activate.
 * @image: For most types, the name of the icon in the icon theme to
 * display next to the menu item, if required.  For the %E_POPUP_IMAGE
 * type, it is a pointer to the &GtkWidget instead.
 * @visible: Visibility mask.  Used together with the &EPopupTarget mask
 * to determine if the item should be part of the menu or not.
 * @enable: Sensitivity mask. Similar to the visibility mask, but
 * currently unimplemented.
 * @popup: Used by e-popup to reference the parent object from
 * callbacks.
 *
 * The EPopupItem defines a single popup menu item, or submenu item,
 * or menu separator based on the @type.  Any number of these are
 * merged at popup display type to form the popup menu.
 *
 * The application may extend this structure using simple C structure
 * containers to add any additional fields it may require.
 */
struct _EPopupItem {
	enum _e_popup_t type;
	char *path;		/* absolute path! must sort ascii-lexographically into the right spot */
	char *label;
	EPopupActivateFunc activate;
	void *user_data;	/* user data, not passed directly to @activate */
	void *image;		/* char* for item type, GtkWidget * for image type */
	guint32 visible;		/* visibility mask */
	guint32 enable;		/* sensitivity mask, unimplemented */
};

/**
 * struct EPopupTarget - A popup menu target definition.
 *
 * @popup: The parent popup object, used for virtual methods on the target.
 * @widget: The parent widget, where available.  In some cases the
 * type of this object is part of the published api for the target.
 * @type: The target type.  This will be defined by the
 * implementation.
 * @mask: Target mask. This is used to sensitise and show items
 * based on their definition in EPopupItem.
 *
 * An EPopupTarget defines the context for a specific popup menu
 * instance.  The root target object is abstract, and it is up to
 * sub-classes of &EPopup to define the additional fields required to
 * make it usable.
 */
struct _EPopupTarget {
	struct _EPopup *popup;	/* used for virtual methods */

	struct _GtkWidget *widget;	/* used if you need a parent toplevel, if available */
	guint32 type;		/* targe type, for implementors */

	guint32 mask;		/* depends on type, visibility mask */

	/* implementation fields follow */
};

/**
 * struct _EPopup - A Popup menu manager.
 * 
 * @object: Superclass, GObject.
 * @priv: Private data.
 * @menuid: The id of this menu instance.
 * @target: The current target during the display of the popup menu.
 * 
 * The EPopup manager object.  Each popup menu is built using this
 * one-off object which is created each time the popup is invoked.
 */
struct _EPopup {
	GObject object;

	struct _EPopupPrivate *priv;

	char *menuid;

	EPopupTarget *target;
};

/**
 * struct _EPopupClass - 
 * 
 * @object_class: Superclass type.
 * @factories: A list of factories for this particular class of popup
 * menu.
 * @target_free: Virtual method to free the popup target.  The base
 * class frees the allocation and unrefs the popup pointer
 * structure.
 * 
 * The EPopup class definition.  This should be sub-classed for each
 * component that wants to provide hookable popup menus.  The
 * sub-class only needs to know how to allocate and free the various target
 * types it supports.
 */
struct _EPopupClass {
	GObjectClass object_class;

	EDList factories;

	void (*target_free)(EPopup *ep, EPopupTarget *t);
};

GType e_popup_get_type(void);

/* Static class methods */
EPopupFactory *e_popup_class_add_factory(EPopupClass *klass, const char *menuid, EPopupFactoryFunc func, void *data);
void e_popup_class_remove_factory(EPopupClass *klass, EPopupFactory *f);

EPopup *e_popup_construct(EPopup *, const char *menuid);

void e_popup_add_items(EPopup *, GSList *items, EPopupItemsFunc freefunc, void *data);

void e_popup_add_static_items(EPopup *emp, EPopupTarget *target);
/* do not call e_popup_create_menu, it can leak structures if not used right */
struct _GtkMenu *e_popup_create_menu(EPopup *, EPopupTarget *, guint32 hide_mask, guint32 disable_mask);
struct _GtkMenu *e_popup_create_menu_once(EPopup *emp, EPopupTarget *, guint32 hide_mask, guint32 disable_mask);

void *e_popup_target_new(EPopup *, int type, size_t size);
void e_popup_target_free(EPopup *, void *);

/* ********************************************************************** */

/* popup plugin target, they are closely integrated */

/* To implement a basic popup menu plugin, you just need to subclass
   this and initialise the class target type tables */

#include "e-util/e-plugin.h"

typedef struct _EPopupHookMenu EPopupHookMenu;
typedef struct _EPopupHook EPopupHook;
typedef struct _EPopupHookClass EPopupHookClass;

typedef struct _EPluginHookTargetMap EPopupHookTargetMap;
typedef struct _EPluginHookTargetKey EPopupHookTargetMask;

typedef void (*EPopupHookFunc)(struct _EPlugin *plugin, EPopupTarget *target);

/**
 * struct _EPopupHookMenu - 
 * 
 * @hook: Parent pointer.
 * @id: The identifier of the menu to which these items belong.
 * @target_type: The target number of the type of target these menu
 * items expect. It will generally also be defined by the menu id.
 * @items: A list of EPopupItems.
 * 
 * The structure used to keep track of all of the items that a plugin
 * wishes to add to a given menu. This is used internally by a factory
 * method set on EPlugin to add the right menu items to a given menu.
 */
struct _EPopupHookMenu {
	struct _EPopupHook *hook; /* parent pointer */
	char *id;		/* target menu id for these menu items */
	int target_type;	/* target type of this menu */
	GSList *items;		/* items to add to menu */
};

/**
 * struct _EPopupHook - A popup menu hook.
 * 
 * @hook: Superclass.
 * @menus: A list of EPopupHookMenus, for all menus registered on
 * this hook type.
 *
 * The EPopupHook class loads and manages the meta-data required to
 * map plugin definitions to physical menus.
 */
struct _EPopupHook {
	EPluginHook hook;

	GSList *menus;
};

/**
 * struct _EPopupHookClass - 
 * 
 * @hook_class: Superclass.
 * @target_map: Table of EPluginHookTargetMaps which enumerate the
 * target types and enable bits of the implementing class.
 * @popup_class: The EPopupClass of the corresponding popup manager
 * for the implementing class.
 * 
 * The EPopupHookClass is a concrete class, however it is empty on its
 * own.  It needs to be sub-classed and initialised appropriately.
 *
 * The EPluginHookClass.id must be set to the name and version of the
 * hook handler itself.  The @target_map must be initialised with the
 * data required to enumerate the target types and enable flags
 * supported by the implementing class.
 */
struct _EPopupHookClass {
	EPluginHookClass hook_class;

	/* EPopupHookTargetMap by .type */
	GHashTable *target_map;
	/* the popup class these popups belong to */
	EPopupClass *popup_class;
};

GType e_popup_hook_get_type(void);

/* for implementors */
void e_popup_hook_class_add_target_map(EPopupHookClass *klass, const EPopupHookTargetMap *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_POPUP_H__ */
