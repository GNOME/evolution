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

#ifndef __E_MENU_H__
#define __E_MENU_H__

#include <glib-object.h>
#include "libedataserver/e-msgport.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* This is an abstract popup menu management/merging class.

   To implement your own popup menu system, just create your own
   target types and implement the target free method. */

typedef struct _EMenu EMenu;
typedef struct _EMenuClass EMenuClass;

typedef struct _EMenuItem EMenuItem;
typedef struct _EMenuUIFile EMenuUIFile;
typedef struct _EMenuPixmap EMenuPixmap;

typedef struct _EMenuFactory EMenuFactory; /* anonymous type */
typedef struct _EMenuTarget EMenuTarget;

typedef void (*EMenuFactoryFunc)(EMenu *emp, void *data);
typedef void (*EMenuActivateFunc)(EMenu *, EMenuItem *, void *data);
typedef void (*EMenuToggleActivateFunc)(EMenu *, EMenuItem *, int state, void *data);
typedef void (*EMenuItemsFunc)(EMenu *, GSList *items, GSList *uifiles, GSList *pixmaps, void *data);

/**
 * enum _e_menu_t - Menu item type.
 * 
 * @E_MENU_ITEM: Normal menu item.
 * @E_MENU_TOGGLE: Toggle menu item.
 * @E_MENU_RADIO: unimplemented.
 * @E_MENU_TYPE_MASK: Mask used to separate item type from option bits.
 * @E_MENU_ACTIVE: Whether a toggle item is active.
 * 
 * The type of menu items which are supported by the menu system.
 **/
enum _e_menu_t {
	E_MENU_ITEM = 0,
	E_MENU_TOGGLE,
	E_MENU_RADIO,
	E_MENU_TYPE_MASK = 0xffff,
	E_MENU_ACTIVE = 0x10000,
};

/**
 * struct _EMenuItem - A BonoboUI menu item.
 * 
 * @type: Menu item type.  %E_MENU_ITEM or %E_MENU_TOGGLE.
 * @path: BonoboUI Path to the menu item.
 * @verb: BonoboUI verb for the menu item.
 * @activate: Callback when the menu item is selected.  This will be a
 * EMenuToggleActivateFunc for toggle items or EMenuActivateFunc for
 * normal items.
 * @user_data: User data for item.
 * @visible: Visibility mask, unimplemented.
 * @enable: Sensitivity mask, combined with the target mask.
 * 
 * An EMenuItem defines a single menu item.  This menu item is used to
 * hook onto callbacks from the bonobo menus, but not to build or
 * merge the menu itself.
 **/
struct _EMenuItem {
	enum _e_menu_t type;
	char *path;		/* full path?  can we just create it from verb? */
	char *verb;		/* command verb */
	GCallback activate;	/* depends on type, the bonobo activate callback */
	void *user_data;	/* up to caller to use */
	guint32 visible;	/* is visible mask */
	guint32 enable;		/* is enable mask */
};

/**
 * struct _EMenuPixmap - A menu icon holder.
 * 
 * @command: The path to the command or verb to which this pixmap belongs.
 * @name: The name of the icon.  Either an icon-theme name or the full
 * pathname of the icon.
 * @size: The e-icon-factory icon size.
 * @pixmap: The pixmap converted to XML format.  If not set, then EMenu will
 * create it as required.  This must be freed if set in the free function.
 *
 * Used to track all pixmap items used in menus.  These need to be
 * supplied separately from the menu definition.
 **/
struct _EMenuPixmap {
	char *command;
	char *name;
	int size;
	char *pixmap;
};

/**
 * struct _EMenuUIFile - A meu UI file holder.
 * 
 * @appdir: TODO; should this be handled internally.
 * @appname: TODO; should this be handled internally.
 * @filename: The filename of the BonoboUI XML menu definition.
 * 
 * These values are passed directly to bonobo_ui_util_set_ui() when
 * the menu is activated.
 **/
struct _EMenuUIFile {
	char *appdir;
	char *appname;
	char *filename;
};

/**
 * struct _EMenuTarget - A BonoboUI menu target definition.
 * 
 * @menu: The parent menu object, used for virtual methods on the target.
 * @widget: The parent widget where available.  In some cases the type
 * of this object is part of the published api for the target, in
 * others it is merely a GtkWidget from which you can find the
 * toplevel widget.
 * @type: Target type.  This will be defined by the implementation.
 * @mask: Target mask.  This is used to sensitise show items based on
 * their definition in EMenuItem.
 * 
 * An EMenuTarget defines the context for a specific view instance.
 * It is used to enable and show menu items, and to provide contextual
 * data to menu invocations.
 **/
struct _EMenuTarget {
	struct _EMenu *menu;	/* used for virtual methods */

	struct _GtkWidget *widget;	/* used if you need a parent toplevel, if available */
	guint32 type;		/* for implementors */

	guint32 mask;		/* enable/visible mask */
	
	/* implementation fields follow */
};

/**
 * struct _EMenu - A BonoboUI menu manager object.
 * 
 * @object: Superclass.
 * @priv: Private data.
 * @menuid: The id of this menu instance.
 * @uic: The current BonoboUIComponent which stores the actual menu
 * items this object manages.
 * @target: The current target for the view.
 * 
 * The EMenu manager object manages the mappings between EMenuItems
 * and the BonoboUI menus loaded from UI files.
 **/
struct _EMenu {
	GObject object;
	struct _EMenuPrivate *priv;

	char *menuid;
	struct _BonoboUIComponent *uic;
	EMenuTarget *target;
};

/**
 * struct _EMenuClass - 
 * 
 * @object_class: Superclass type.
 * @factories: A list of factories for this particular class of main menu.
 * @target_free: Virtual method to free the menu target.  The base
 * class free method frees the allocation and unrefs the EMenu parent
 * pointer.
 *
 * The EMenu class definition.  This should be sub-classed for each
 * component that wants to provide hookable main menus.  The subclass
 * only needs to know how to allocate and free the various target
 * types it supports.
 **/
struct _EMenuClass {
	GObjectClass object_class;

	EDList factories;

	void (*target_free)(EMenu *ep, EMenuTarget *t);
};

GType e_menu_get_type(void);

/* Static class methods */
EMenuFactory *e_menu_class_add_factory(EMenuClass *klass, const char *menuid, EMenuFactoryFunc func, void *data);
void e_menu_class_remove_factory(EMenuClass *klass, EMenuFactory *f);

EMenu *e_menu_construct(EMenu *menu, const char *menuid);

void e_menu_add_ui(EMenu *, const char *appdir, const char *appname, const char *filename);
void e_menu_add_pixmap(EMenu *, const char *cmd, const char *name, int size);

void *e_menu_add_items(EMenu *emp, GSList *items, GSList *uifiles, GSList *pixmaps, EMenuItemsFunc freefunc, void *data);
void e_menu_remove_items(EMenu *emp, void *handle);

void e_menu_activate(EMenu *, struct _BonoboUIComponent *uic, int act);
void e_menu_update_target(EMenu *, void *);

void *e_menu_target_new(EMenu *, int type, size_t size);
void e_menu_target_free(EMenu *, void *);

/* ********************************************************************** */

/* menu plugin, they are closely integrated */

/* To implement a basic menu plugin, you just need to subclass
   this and initialise the class target type tables */

#include "e-util/e-plugin.h"

typedef struct _EMenuHookPixmap EMenuHookPixmap;
typedef struct _EMenuHookMenu EMenuHookMenu;
typedef struct _EMenuHook EMenuHook;
typedef struct _EMenuHookClass EMenuHookClass;

typedef struct _EPluginHookTargetMap EMenuHookTargetMap;
typedef struct _EPluginHookTargetKey EMenuHookTargetMask;

typedef void (*EMenuHookFunc)(struct _EPlugin *plugin, EMenuTarget *target);

/**
 * struct _EMenuHookMenu - A group of items targetting a specific menu.
 * 
 * @hook: Parent pointer.
 * @id: The identifier of the menu or view to which these items belong.
 * @target_type: The target number of the type of target these menu
 * items expect.  This will be defined by menu itself.
 * @items: A list of EMenuItems.
 * @uis: A list of filenames of the BonoboUI files that need to be
 * loaded for an active view.
 * @pixmaps: A list of EMenuHookPixmap structures for the menus.
 * 
 * This structure is used to keep track of all of the items that a
 * plugin wishes to add to specific menu.  This is used internally by
 * a factory method defined by the EMenuHook to add the right menu
 * items to a given view.
 **/
struct _EMenuHookMenu {
	struct _EMenuHook *hook; /* parent pointer */
	char *id;		/* target menu id for these menu items */
	int target_type;	/* target type, not used */
	GSList *items;		/* items to add to menu */
	GSList *uis;		/* ui files */
	GSList *pixmaps;	/* pixmap descriptors */
};

/**
 * struct _EMenuHook - A BonoboUI menu hook.
 * 
 * @hook: Superclass.
 * @menus: A list of EMenuHookMenus for all menus registered on this
 * hook type.
 *
 * The EMenuHook class loads and manages the meta-data to required to
 * map plugin definitions to physical menus.
 **/
struct _EMenuHook {
	EPluginHook hook;

	GSList *menus;
};

/**
 * struct _EMenuHookClass - Menu hook type.
 * 
 * @hook_class: Superclass type.
 * @target_map: Table of EluginHookTargetMaps which enumerate the
 * target types and enable bits of the implementing class.
 * @menu_class: The EMenuClass of the corresponding popup manager for
 * implementing the class.
 * 
 * The EMenuHookClass is an empty concrete class.  It must be
 * subclassed and initialised appropriately to perform useful work.
 *
 * The EPluginHookClass.id must be set to the name and version of the
 * hook handler the implementation defines.  The @target_map must be
 * initialised with the data required to enumerate the target types
 * and enable flags supported by the implementing class.
 **/
struct _EMenuHookClass {
	EPluginHookClass hook_class;

	/* EMenuHookTargetMap by .type */
	GHashTable *target_map;
	/* the menu class these menus belong to */
	EMenuClass *menu_class;
};

GType e_menu_hook_get_type(void);

/* for implementors */
void e_menu_hook_class_add_target_map(EMenuHookClass *klass, const EMenuHookTargetMap *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_MENU_H__ */
