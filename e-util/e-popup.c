/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>

#include "e-popup.h"

#include <e-util/e-icon-factory.h>

#include <libgnome/gnome-i18n.h>

struct _EPopupFactory {
	struct _EPopupFactory *next, *prev;

	char *menuid;
	EPopupFactoryFunc factory;
	void *factory_data;
};

/* Used for the "activate" signal callback data to re-map to the api */
struct _item_node {
	struct _item_node *next;

	EPopupItem *item;
	struct _menu_node *menu;
};

/* Stores all the items added */
struct _menu_node {
	struct _menu_node *next, *prev;

	EPopup *popup;

	GSList *menu;
	EPopupItemsFunc freefunc;
	void *data;

	struct _item_node *items;
};

struct _EPopupPrivate {
	EDList menus;
};

static GObjectClass *ep_parent;

static void
ep_init(GObject *o)
{
	EPopup *emp = (EPopup *)o;
	struct _EPopupPrivate *p;

	p = emp->priv = g_malloc0(sizeof(struct _EPopupPrivate));

	e_dlist_init(&p->menus);
}

static void
ep_finalise(GObject *o)
{
	EPopup *emp = (EPopup *)o;
	struct _EPopupPrivate *p = emp->priv;
	struct _menu_node *mnode, *nnode;

	mnode = (struct _menu_node *)p->menus.head;
	nnode = mnode->next;
	while (nnode) {
		struct _item_node *inode;

		if (mnode->freefunc)
			mnode->freefunc(emp, mnode->menu, mnode->data);

		/* free item activate callback data */
		inode = mnode->items;
		while (inode) {
			struct _item_node *nnode = inode->next;

			g_free(inode);
			inode = nnode;
		}

		g_free(mnode);
		mnode = nnode;
		nnode = nnode->next;
	}

	if (emp->target)
		e_popup_target_free(emp, emp->target);

	g_free(emp->menuid);

	g_free(p);

	((GObjectClass *)ep_parent)->finalize(o);
}

static void
ep_target_free(EPopup *ep, EPopupTarget *t)
{
	g_free(t);
	g_object_unref(ep);
}

static void
ep_class_init(GObjectClass *klass)
{
	printf("EPopup class init %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type));

	klass->finalize = ep_finalise;
	((EPopupClass *)klass)->target_free = ep_target_free;
}

static void
ep_base_init(GObjectClass *klass)
{
	e_dlist_init(&((EPopupClass *)klass)->factories);
}

/**
 * e_popup_get_type:
 * 
 * Standard GObject type function.
 * 
 * Return value: The EPopup object type.
 **/
GType
e_popup_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EPopupClass),
			(GBaseInitFunc)ep_base_init, NULL,
			(GClassInitFunc)ep_class_init, NULL, NULL,
			sizeof(EPopup), 0,
			(GInstanceInitFunc)ep_init
		};
		ep_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EPopup", &info, 0);
	}

	return type;
}

/**
 * e_popup_construct:
 * @ep: An instantiated but uninitialised EPopup.
 * @menuid: The menu identifier.
 * 
 * Construct the base popup instance with standard parameters.
 * 
 * Return value: Returns @ep.
 **/
EPopup *e_popup_construct(EPopup *ep, const char *menuid)
{
	ep->menuid = g_strdup(menuid);

	return ep;
}

/**
 * e_popup_add_items:
 * @emp: An EPopup derived object.
 * @items: A list of EPopupItem's to add to the current popup menu.
 * @freefunc: A function which will be called when the items are no
 * longer needed.
 * @data: user-data passed to @freefunc, and passed to all activate
 * methods.
 * 
 * Add new EPopupItems to the menus.  Any with the same path
 * will override previously defined menu items, at menu building
 * time.  This may be called any number of times before the menu is
 * built to create a complex heirarchy of menus.
 **/
void
e_popup_add_items(EPopup *emp, GSList *items, EPopupItemsFunc freefunc, void *data)
{
	struct _menu_node *node;

	node = g_malloc0(sizeof(*node));
	node->menu = items;
	node->freefunc = freefunc;
	node->data = data;
	node->popup = emp;

	e_dlist_addtail(&emp->priv->menus, (EDListNode *)node);
}

static void
ep_add_static_items(EPopup *emp)
{
	struct _EPopupFactory *f;
	EPopupClass *klass = (EPopupClass *)G_OBJECT_GET_CLASS(emp);

	if (emp->menuid == NULL || emp->target == NULL)
		return;

	/* setup the menu itself */
	f = (struct _EPopupFactory *)klass->factories.head;
	while (f->next) {
		if (f->menuid == NULL
		    || !strcmp(f->menuid, emp->menuid)) {
			f->factory(emp, f->factory_data);
		}
		f = f->next;
	}
}

static int
ep_cmp(const void *ap, const void *bp)
{
	struct _item_node *a = *((void **)ap);
	struct _item_node *b = *((void **)bp);

	return strcmp(a->item->path, b->item->path);
}

static void
ep_activate(GtkWidget *w, struct _item_node *inode)
{
	inode->item->activate(inode->menu->popup, inode->item, inode->menu->data);
}

/**
 * e_popup_create:
 * @emp: An EPopup derived object.
 * @target: popup target, if set, then factories will be invoked.
 * This is then owned by the menu.
 * @hide_mask: used to hide menu items, not sure of it's utility,
 * since you could just 'not add them' in the first place.  Saves
 * copying logic anyway.
 * @disable_mask: used to disable menu items.
 *
 * All of the menu items registered on @emp are sorted by path, and
 * then converted into a menu heirarchy.
 *
 * Return value: A GtkMenu which can be popped up when ready.
 **/
GtkMenu *
e_popup_create_menu(EPopup *emp, EPopupTarget *target, guint32 hide_mask, guint32 disable_mask)
{
	struct _EPopupPrivate *p = emp->priv;
	struct _menu_node *mnode, *nnode;
	GPtrArray *items = g_ptr_array_new();
	GSList *l;
	GString *ppath = g_string_new("");
	GtkMenu *topmenu;
	GHashTable *menu_hash = g_hash_table_new(g_str_hash, g_str_equal),
		*group_hash = g_hash_table_new(g_str_hash, g_str_equal);
	int i;

	emp->target = target;
	ep_add_static_items(emp);

	/* FIXME: need to override old ones with new names */
	mnode = (struct _menu_node *)p->menus.head;
	nnode = mnode->next;
	while (nnode) {
		for (l=mnode->menu; l; l = l->next) {
			struct _item_node *inode = g_malloc0(sizeof(*inode));

			inode->item = l->data;
			inode->menu = mnode;
			inode->next = mnode->items;
			mnode->items = inode;

			g_ptr_array_add(items, inode);
		}
		mnode = nnode;
		nnode = nnode->next;
	}

	qsort(items->pdata, items->len, sizeof(items->pdata[0]), ep_cmp);

	topmenu = (GtkMenu *)gtk_menu_new();
	for (i=0;i<items->len;i++) {
		GtkWidget *label;
		struct _item_node *inode = items->pdata[i];
		struct _EPopupItem *item = inode->item;
		GtkMenu *thismenu;
		GtkMenuItem *menuitem;
		char *tmp;

		/* for bar's, the mask is exclusive or */
		if (item->visible) {
			if ((item->type & E_POPUP_TYPE_MASK) == E_POPUP_BAR) {
				if ((item->visible & hide_mask) == item->visible)
					continue;
			} else if (item->visible & hide_mask)
				continue;
		}

		g_string_truncate(ppath, 0);
		tmp = strrchr(item->path, '/');
		if (tmp) {
			g_string_append_len(ppath, item->path, tmp-item->path);
			thismenu = g_hash_table_lookup(menu_hash, ppath->str);
			g_assert(thismenu != NULL);
		} else {
			thismenu = topmenu;
		}

		switch (item->type & E_POPUP_TYPE_MASK) {
		case E_POPUP_ITEM:
			if (item->image) {
				GdkPixbuf *pixbuf;
				GtkWidget *image;
				
				pixbuf = e_icon_factory_get_icon ((char *)item->image, E_ICON_SIZE_MENU);
				image = gtk_image_new_from_pixbuf (pixbuf);
				g_object_unref (pixbuf);

				gtk_widget_show(image);
				menuitem = (GtkMenuItem *)gtk_image_menu_item_new();
				gtk_image_menu_item_set_image((GtkImageMenuItem *)menuitem, image);
			} else {
				menuitem = (GtkMenuItem *)gtk_menu_item_new();
			}
			break;
		case E_POPUP_TOGGLE:
			menuitem = (GtkMenuItem *)gtk_check_menu_item_new();
			gtk_check_menu_item_set_active((GtkCheckMenuItem *)menuitem, item->type & E_POPUP_ACTIVE);
			break;
		case E_POPUP_RADIO:
			menuitem = (GtkMenuItem *)gtk_radio_menu_item_new(g_hash_table_lookup(group_hash, ppath->str));
			/* FIXME: need to strdup the string */
			g_hash_table_insert(group_hash, ppath->str, gtk_radio_menu_item_get_group((GtkRadioMenuItem *)menuitem));
			gtk_check_menu_item_set_active((GtkCheckMenuItem *)menuitem, item->type & E_POPUP_ACTIVE);
			break;
		case E_POPUP_IMAGE:
			menuitem = (GtkMenuItem *)gtk_image_menu_item_new();
			gtk_image_menu_item_set_image((GtkImageMenuItem *)menuitem, item->image);
			break;
		case E_POPUP_SUBMENU: {
			GtkMenu *submenu = (GtkMenu *)gtk_menu_new();

			g_hash_table_insert(menu_hash, item->path, submenu);
			menuitem = (GtkMenuItem *)gtk_menu_item_new();
			gtk_menu_item_set_submenu(menuitem, (GtkWidget *)submenu);
			break; }
		case E_POPUP_BAR:
			/* TODO: double-bar, end-bar stuff? */
			menuitem = (GtkMenuItem *)gtk_separator_menu_item_new();
			break;
		default:
			continue;
		}

		if (item->label) {
			label = gtk_label_new_with_mnemonic(_(item->label));
			gtk_misc_set_alignment((GtkMisc *)label, 0.0, 0.5);
			gtk_widget_show(label);
			gtk_container_add((GtkContainer *)menuitem, label);
		}

		if (item->activate)
			g_signal_connect(menuitem, "activate", G_CALLBACK(ep_activate), inode);

		gtk_menu_shell_append((GtkMenuShell *)thismenu, (GtkWidget *)menuitem);

		if (item->visible & disable_mask)
			gtk_widget_set_sensitive((GtkWidget *)menuitem, FALSE);

		gtk_widget_show((GtkWidget *)menuitem);
	}

	g_string_free(ppath, TRUE);
	g_ptr_array_free(items, TRUE);
	g_hash_table_destroy(menu_hash);
	g_hash_table_destroy(group_hash);

	return topmenu;
}

static void
ep_popup_done(GtkWidget *w, EPopup *emp)
{
	gtk_widget_destroy(w);
	if (emp->target) {
		e_popup_target_free(emp, emp->target);
		emp->target = NULL;
	}
	g_object_unref(emp);
}

/**
 * e_popup_create_menu_once:
 * @emp: EPopup, once the menu is shown, this cannot be
 * considered a valid pointer.
 * @target: If set, the target of the selection.  Static menu
 * items will be added.  The target will be freed once complete.
 * @hide_mask: 
 * @disable_mask: 
 * 
 * Like popup_create_menu, but automatically sets up the menu
 * so that it is destroyed once a selection takes place, and
 * the EPopup is unreffed.  This is the normal entry point as it
 * automates most memory management for popup menus.
 * 
 * Return value: A menu, to popup.
 **/
GtkMenu *
e_popup_create_menu_once(EPopup *emp, EPopupTarget *target, guint32 hide_mask, guint32 disable_mask)
{
	GtkMenu *menu;

	menu = e_popup_create_menu(emp, target, hide_mask, disable_mask);

	g_signal_connect(menu, "selection_done", G_CALLBACK(ep_popup_done), emp);

	return menu;
}

/* ********************************************************************** */

/**
 * e_popup_class_add_factory:
 * @klass: The EPopup derived class which you're interested in.
 * @menuid: The identifier of the menu you're interested in, or NULL
 * to be called for all menus on this class.
 * @func: The factory called when the menu @menuid is being created.
 * @data: User-data for the factory callback.
 *
 * This is a class-static method used to register factory callbacks
 * against specific menu's.
 * 
 * The factory method will be invoked before the menu is created.
 * This way, the factory may add any additional menu items it wishes
 * based on the context supplied in the @target.
 * 
 * Return value: A handle to the factory which can be used to remove
 * it later.
 **/
EPopupFactory *
e_popup_class_add_factory(EPopupClass *klass, const char *menuid, EPopupFactoryFunc func, void *data)
{
	struct _EPopupFactory *f = g_malloc0(sizeof(*f));

	f->menuid = g_strdup(menuid);
	f->factory = func;
	f->factory_data = data;
	e_dlist_addtail(&klass->factories, (EDListNode *)f);

	return f;
}

/**
 * e_popup_class_remove_factory:
 * @klass: The EPopup derived class.
 * @f: The factory handle returned by e_popup_class_add_factory().
 * 
 * Remove a popup menu factory. If it has not been added, or it has
 * already been removed, then the result is undefined (i.e. it will
 * crash).
 *
 * Generally factories are static for the life of the application, and
 * so do not need to be removed.
 **/
void
e_popup_class_remove_factory(EPopupClass *klass, EPopupFactory *f)
{
	e_dlist_remove((EDListNode *)f);
	g_free(f->menuid);
	g_free(f);
}

/**
 * e_popup_target_new:
 * @ep: An EPopup derived object.
 * @type: type, defined by the implementing class.
 * @size: The size of memory to allocate for the target.  It must be
 * equal or greater than the size of EPopupTarget.
 * 
 * Allocate a new popup target suitable for this popup type.
 **/
void *e_popup_target_new(EPopup *ep, int type, size_t size)
{
	EPopupTarget *t;

	g_assert(size >= sizeof(EPopupTarget));

	t = g_malloc0(size);
	t->popup = ep;
	g_object_ref(ep);
	t->type = type;

	return t;
}

/**
 * e_popup_target_free:
 * @ep: An EPopup derived object.
 * @o: The target, previously allocated by e_popup_target_new().
 * 
 * Free the target against @ep. Note that targets are automatically
 * freed if they are passed to the menu creation functions, so this is
 * only required if you are using the target for other purposes.
 **/
void
e_popup_target_free(EPopup *ep, void *o)
{
	EPopupTarget *t = o;

	((EPopupClass *)G_OBJECT_GET_CLASS(ep))->target_free(ep, t);
}

/* ********************************************************************** */

/* Popup menu plugin handler */

/*
<e-plugin
  class="com.ximian.mail.plugin.popup:1.0"
  id="com.ximian.mail.plugin.popup.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="com.ximian.mail.popupMenu:1.0"
        handler="HandlePopup">
  <menu id="any" target="select">
   <item
    type="item|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    visible="select_one"
    activate="ep_view_emacs"/>
  </menu>
  </extension>

*/

static void *emph_parent_class;
#define emph ((EPopupHook *)eph)

/* must have 1:1 correspondence with e-popup types in order */
static const EPluginHookTargetKey emph_item_types[] = {
	{ "item", E_POPUP_ITEM },
	{ "toggle", E_POPUP_TOGGLE },
	{ "radio", E_POPUP_RADIO },
	{ "image", E_POPUP_IMAGE },
	{ "submenu", E_POPUP_SUBMENU },
	{ "bar", E_POPUP_BAR },
	{ 0 }
};

static void
emph_popup_activate(EPopup *ep, EPopupItem *item, void *data)
{
	EPopupHook *hook = data;

	e_plugin_invoke(hook->hook.plugin, (char *)item->user_data, ep->target);
}

static void
emph_popup_factory(EPopup *emp, void *data)
{
	struct _EPopupHookMenu *menu = data;

	printf("popup factory called %s mask %08x\n", menu->id?menu->id:"all menus", emp->target->mask);

	/* If we're disabled, then don't add the menu's. */
	if (emp->target->type != menu->target_type
	    || !menu->hook->hook.plugin->enabled)
		return;

	if (menu->items)
		e_popup_add_items(emp, menu->items, NULL, menu->hook);
}

static void
emph_free_item(struct _EPopupItem *item)
{
	g_free(item->path);
	g_free(item->label);
	g_free(item->image);
	g_free(item->user_data);
	g_free(item);
}

static void
emph_free_menu(struct _EPopupHookMenu *menu)
{
	g_slist_foreach(menu->items, (GFunc)emph_free_item, NULL);
	g_slist_free(menu->items);

	g_free(menu->id);
	g_free(menu);
}

static struct _EPopupItem *
emph_construct_item(EPluginHook *eph, EPopupHookMenu *menu, xmlNodePtr root, EPopupHookTargetMap *map)
{
	struct _EPopupItem *item;

	printf("  loading menu item\n");
	item = g_malloc0(sizeof(*item));
	if ((item->type = e_plugin_hook_id(root, emph_item_types, "type")) == -1
	    || item->type == E_POPUP_IMAGE)
		goto error;
	item->path = e_plugin_xml_prop(root, "path");
	item->label = e_plugin_xml_prop_domain(root, "label", eph->plugin->domain);
	item->image = e_plugin_xml_prop(root, "icon");
	item->visible = e_plugin_hook_mask(root, map->mask_bits, "visible");
	item->enable = e_plugin_hook_mask(root, map->mask_bits, "enable");
	item->user_data = e_plugin_xml_prop(root, "activate");

	item->activate = emph_popup_activate;

	if (item->user_data == NULL)
		goto error;

	printf("   path=%s\n", item->path);
	printf("   label=%s\n", item->label);

	return item;
error:
	printf("error!\n");
	emph_free_item(item);
	return NULL;
}

static struct _EPopupHookMenu *
emph_construct_menu(EPluginHook *eph, xmlNodePtr root)
{
	struct _EPopupHookMenu *menu;
	xmlNodePtr node;
	EPopupHookTargetMap *map;
	EPopupHookClass *klass = (EPopupHookClass *)G_OBJECT_GET_CLASS(eph);
	char *tmp;

	printf(" loading menu\n");
	menu = g_malloc0(sizeof(*menu));
	menu->hook = (EPopupHook *)eph;

	tmp = xmlGetProp(root, "target");
	if (tmp == NULL)
		goto error;
	map = g_hash_table_lookup(klass->target_map, tmp);
	xmlFree(tmp);
	if (map == NULL)
		goto error;

	menu->target_type = map->id;
	menu->id = e_plugin_xml_prop(root, "id");
	node = root->children;
	while (node) {
		if (0 == strcmp(node->name, "item")) {
			struct _EPopupItem *item;

			item = emph_construct_item(eph, menu, node, map);
			if (item)
				menu->items = g_slist_append(menu->items, item);
		}
		node = node->next;
	}

	return menu;
error:
	emph_free_menu(menu);
	return NULL;
}

static int
emph_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;
	EPopupClass *klass;

	printf("loading popup hook\n");

	if (((EPluginHookClass *)emph_parent_class)->construct(eph, ep, root) == -1)
		return -1;

	klass = ((EPopupHookClass *)G_OBJECT_GET_CLASS(eph))->popup_class;

	node = root->children;
	while (node) {
		if (strcmp(node->name, "menu") == 0) {
			struct _EPopupHookMenu *menu;

			menu = emph_construct_menu(eph, node);
			if (menu) {
				e_popup_class_add_factory(klass, menu->id, emph_popup_factory, menu);
				emph->menus = g_slist_append(emph->menus, menu);
			}
		}
		node = node->next;
	}

	eph->plugin = ep;

	return 0;
}

static void
emph_finalise(GObject *o)
{
	EPluginHook *eph = (EPluginHook *)o;

	g_slist_foreach(emph->menus, (GFunc)emph_free_menu, NULL);
	g_slist_free(emph->menus);

	((GObjectClass *)emph_parent_class)->finalize(o);
}

static void
emph_class_init(EPluginHookClass *klass)
{
	((GObjectClass *)klass)->finalize = emph_finalise;
	klass->construct = emph_construct;

	/* this is actually an abstract implementation but list it anyway */
	klass->id = "com.ximian.evolution.popup:1.0";

	printf("EPopupHook: init class %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type));

	((EPopupHookClass *)klass)->target_map = g_hash_table_new(g_str_hash, g_str_equal);
	((EPopupHookClass *)klass)->popup_class = g_type_class_ref(e_popup_get_type());
}

/**
 * e_popup_hook_get_type:
 * 
 * Standard GObject function to get the object type.  Used to subclass
 * EPopupHook.
 *
 * Return value: The type of the popup hook class.
 **/
GType
e_popup_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EPopupHookClass), NULL, NULL, (GClassInitFunc) emph_class_init, NULL, NULL,
			sizeof(EPopupHook), 0, (GInstanceInitFunc) NULL,
		};

		emph_parent_class = g_type_class_ref(e_plugin_hook_get_type());
		type = g_type_register_static(e_plugin_hook_get_type(), "EPopupHook", &info, 0);
	}
	
	return type;
}

/**
 * e_popup_hook_class_add_target_map:
 * @klass: The derived EPopupHook class.
 * @map: A map used to describe a single EPopupTarget type for this
 * class.
 * 
 * Add a target map to a concrete derived class of EPopup.  The target
 * map enumerates a single target type and the enable mask bit names,
 * so that the type can be loaded automatically by the EPopup class.
 **/
void e_popup_hook_class_add_target_map(EPopupHookClass *klass, const EPopupHookTargetMap *map)
{
	g_hash_table_insert(klass->target_map, (void *)map->type, (void *)map);
}
