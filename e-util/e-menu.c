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

#include "e-menu.h"

#include <e-util/e-icon-factory.h>

#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-ui-util.h>

#define d(x)

struct _EMenuFactory {
	struct _EMenuFactory *next, *prev;

	char *menuid;
	EMenuFactoryFunc factory;
	void *factory_data;
};

struct _item_node {
	struct _item_node *next;

	EMenuItem *item;
	struct _menu_node *menu;
};

struct _menu_node {
	struct _menu_node *next, *prev;

	EMenu *parent;

	GSList *items;
	GSList *uis;
	GSList *pixmaps;

	EMenuItemsFunc freefunc;
	void *data;

	/* a copy of items wrapped in an item_node, for bonobo
	 * callback mapping */
	struct _item_node *menu;
};

struct _EMenuPrivate {
	EDList menus;
};

static GObjectClass *em_parent;

static void
em_init(GObject *o)
{
	EMenu *emp = (EMenu *)o;
	struct _EMenuPrivate *p;

	p = emp->priv = g_malloc0(sizeof(struct _EMenuPrivate));

	e_dlist_init(&p->menus);
}

static void
em_finalise(GObject *o)
{
	EMenu *em = (EMenu *)o;
	struct _EMenuPrivate *p = em->priv;
	struct _menu_node *mnode;

	if (em->target)
		e_menu_target_free(em, em->target);
	g_free(em->menuid);

	while ((mnode = (struct _menu_node *)e_dlist_remhead(&p->menus))) {
		struct _item_node *inode;

		if (mnode->freefunc)
			mnode->freefunc(em, mnode->items, mnode->uis, mnode->pixmaps, mnode->data);

		inode = mnode->menu;
		while (inode) {
			struct _item_node *nnode = inode->next;

			g_free(inode);
			inode = nnode;
		}

		g_free(mnode);
	}

	g_free(p);

	((GObjectClass *)em_parent)->finalize(o);
}

static void
em_target_free(EMenu *ep, EMenuTarget *t)
{
	g_free(t);
	/* look funny but t has a reference to us */
	g_object_unref(ep);
}

static void
em_class_init(GObjectClass *klass)
{
	d(printf("EMenu class init %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type)));

	klass->finalize = em_finalise;
	((EMenuClass *)klass)->target_free = em_target_free;
}

static void
em_base_init(GObjectClass *klass)
{
	/* each class instance must have its own list, it isn't inherited */
	d(printf("%p: list init\n", klass));
	e_dlist_init(&((EMenuClass *)klass)->factories);
}

/**
 * e_menu_get_type:
 * 
 * Standard GObject type function.  Used to subclass this type only.
 * 
 * Return value: The EMenu object type.
 **/
GType
e_menu_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMenuClass),
			(GBaseInitFunc)em_base_init, NULL,
			(GClassInitFunc)em_class_init,
			NULL, NULL,
			sizeof(EMenu), 0,
			(GInstanceInitFunc)em_init
		};
		em_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EMenu", &info, 0);
	}

	return type;
}

/**
 * e_menu_construct:
 * @em: An instantiated but uninitislied EPopup.
 * @menuid: The unique identifier for this menu.
 * 
 * Construct the base menu instance based on the parameters.
 * 
 * Return value: Returns @em.
 **/
EMenu *e_menu_construct(EMenu *em, const char *menuid)
{
	struct _EMenuFactory *f;
	EMenuClass *klass;

	d(printf("constructing menu '%s'\n", menuid));

	klass = (EMenuClass *)G_OBJECT_GET_CLASS(em);

	d(printf("   class is %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type)));

	em->menuid = g_strdup(menuid);

	/* setup the menu itself based on factories */
	f = (struct _EMenuFactory *)klass->factories.head;
	if (f->next == NULL) {
		d(printf("%p no factories registered on menu\n", klass));
	}

	while (f->next) {
		if (f->menuid == NULL
		    || !strcmp(f->menuid, em->menuid)) {
			d(printf("  calling factory\n"));
			f->factory(em, f->factory_data);
		}
		f = f->next;
	}

	return em;
}

/**
 * e_menu_add_items:
 * @emp: An initialised EMenu.
 * @items: A list of EMenuItems or derived structures defining a group
 * of menu items for this menu.
 * @uifiles: A list of EMenuUIFile objects describing all ui files
 * associated with the items.
 * @pixmaps: A list of EMenuPixmap objects describing all pixmaps
 * associated with the menus.
 * @freefunc: If supplied, called when the menu items are no longer needed.
 * @data: user-data passed to @freefunc and activate callbacks.
 * 
 * Add new EMenuItems to the menu's.  This may be called any number of
 * times before the menu is first activated to hook onto any of the
 * menu items defined for that view.
 *
 * Return value: A handle that can be passed to remove_items as required.
 **/
void *
e_menu_add_items(EMenu *emp, GSList *items, GSList *uifiles, GSList *pixmaps, EMenuItemsFunc freefunc, void *data)
{
	struct _menu_node *node;
	GSList *l;

	node = g_malloc0(sizeof(*node));
	node->parent = emp;
	node->items = items;
	node->uis = uifiles;
	node->pixmaps = pixmaps;
	node->freefunc = freefunc;
	node->data = data;

	for (l=items;l;l=g_slist_next(l)) {
		struct _item_node *inode = g_malloc0(sizeof(*inode));
		EMenuItem *item = l->data;

		inode->item = item;
		inode->menu = node;
		inode->next = node->menu;
		node->menu = inode;
	}

	for (l=pixmaps;l;l=g_slist_next(l)) {
		EMenuPixmap *pixmap = l->data;

		if (pixmap->pixmap == NULL) {
			GdkPixbuf *pixbuf;

			pixbuf = e_icon_factory_get_icon(pixmap->name, pixmap->size);
			if (pixbuf == NULL) {
				g_warning("Unable to load icon '%s'", pixmap->name);
			} else {
				pixmap->pixmap = bonobo_ui_util_pixbuf_to_xml(pixbuf);
				g_object_unref(pixbuf);
			}
		}
	}

	e_dlist_addtail(&emp->priv->menus, (EDListNode *)node);

	/* FIXME: add the menu's to a running menu if it is there? */

	return (void *)node;
}

/**
 * e_menu_remove_items:
 * @emp: 
 * @handle: 
 * 
 * Remove menu items previously added.
 **/
void
e_menu_remove_items(EMenu *emp, void *handle)
{
	struct _menu_node *node = handle;
	struct _item_node *inode;
	GSList *l;

	e_dlist_remove((EDListNode *)node);

	if (emp->uic) {
		for (l = node->items;l;l=g_slist_next(l)) {
			EMenuItem *item = l->data;
		
			bonobo_ui_component_remove_verb(emp->uic, item->verb);
		}
	}

	if (node->freefunc)
		node->freefunc(emp, node->items, node->uis, node->pixmaps, node->data);

	inode = node->menu;
	while (inode) {
		struct _item_node *nnode = inode->next;

		g_free(inode);
		inode = nnode;
	}

	g_free(node);
}

static void
em_activate_toggle(BonoboUIComponent *component, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	struct _item_node *inode = data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	((EMenuToggleActivateFunc)inode->item->activate)(inode->menu->parent, inode->item, state[0] != '0', inode->menu->data);
}

static void
em_activate(BonoboUIComponent *uic, void *data, const char *cname)
{
	struct _item_node *inode = data;

	((EMenuActivateFunc)inode->item->activate)(inode->menu->parent, inode->item, inode->menu->data);
}

/**
 * e_menu_activate:
 * @em: An initialised EMenu.
 * @uic: The BonoboUI component for this views menu's.
 * @act: If %TRUE, then the control is being activated.
 * 
 * This is called by the owner of the component, control, or view to
 * pass on the activate or deactivate control signals.  If the view is
 * being activated then the callbacks and menu items are setup,
 * otherwise they are removed.
 *
 * This should always be called in the strict sequence of activate, then
 * deactivate, repeated any number of times.
 **/
void e_menu_activate(EMenu *em, struct _BonoboUIComponent *uic, int act)
{
	struct _EMenuPrivate *p = em->priv;
	struct _menu_node *mw;
	GSList *l;

	if (act) {
		GArray *verbs;
		int i;

		em->uic = uic;

		verbs = g_array_new(TRUE, FALSE, sizeof(BonoboUIVerb));
		for (mw = (struct _menu_node *)p->menus.head;mw->next;mw=mw->next) {
			struct _item_node *inode;

			for (l = mw->uis; l ; l = g_slist_next(l)) {
				EMenuUIFile *ui = l->data;

				bonobo_ui_util_set_ui(uic, ui->appdir, ui->filename, ui->appname, NULL);
			}

			for (l = mw->pixmaps; l ; l = g_slist_next(l)) {
				EMenuPixmap *pm = l->data;

				if (pm->pixmap)
					bonobo_ui_component_set_prop(uic, pm->command, "pixmap", pm->pixmap, NULL);
			}

			for (inode = mw->menu; inode; inode=inode->next) {
				EMenuItem *item = inode->item;
				BonoboUIVerb *verb;

				d(printf("adding menu verb '%s'\n", item->verb));

				switch (item->type & E_MENU_TYPE_MASK) {
				case E_MENU_ITEM:
					i = verbs->len;
					verbs = g_array_set_size(verbs, i+1);
					verb = &((BonoboUIVerb *)verbs->data)[i];

					verb->cname = item->verb;
					verb->cb = em_activate;
					verb->user_data = inode;
					break;
				case E_MENU_TOGGLE:
					bonobo_ui_component_set_prop(uic, item->path, "state", item->type & E_MENU_ACTIVE?"1":"0", NULL);
					bonobo_ui_component_add_listener(uic, item->verb, em_activate_toggle, inode);
					break;
				}
			}
		}

		if (verbs->len)
			bonobo_ui_component_add_verb_list(uic, (BonoboUIVerb *)verbs->data);

		g_array_free(verbs, TRUE);
	} else {
		for (mw = (struct _menu_node *)p->menus.head;mw->next;mw=mw->next) {
			for (l = mw->items;l;l=g_slist_next(l)) {
				EMenuItem *item = l->data;

				bonobo_ui_component_remove_verb(uic, item->verb);
			}
		}

		em->uic = NULL;
	}
}

/**
 * e_menu_update_target:
 * @em: An initialised EMenu.
 * @tp: Target, after this call the menu owns the target.
 * 
 * Change the target for the menu.  Once the target is changed, the
 * sensitivity state of the menu items managed by @em is re-evaluated
 * and the physical menu's updated to reflect it.
 *
 * This is used by the owner of the menu and view to update the menu
 * system based on user input or changed system state.
 **/
void e_menu_update_target(EMenu *em, void *tp)
{
	struct _EMenuPrivate *p = em->priv;
	EMenuTarget *t = tp;
	guint32 mask = ~0;
	struct _menu_node *mw;
	GSList *l;

	if (em->target && em->target != t)
		e_menu_target_free(em, em->target);

	/* if we unset the target, should we disable/hide all the menu items? */
	em->target = t;
	if (t == NULL)
		return;

	mask = t->mask;

	/* canna do any more capt'n */
	if (em->uic == NULL)
		return;

	for (mw = (struct _menu_node *)p->menus.head;mw->next;mw=mw->next) {
		for (l = mw->items;l;l=g_slist_next(l)) {
			EMenuItem *item = l->data;
			int state;

			d(printf("checking item '%s' mask %08x against target %08x\n", item->verb, item->enable, mask));

			state = (item->enable & mask) == 0;
			bonobo_ui_component_set_prop(em->uic, item->path, "sensitive", state?"1":"0", NULL);
			/* visible? */
		}
	}
}

/* ********************************************************************** */

/**
 * e_menu_class_add_factory:
 * @klass: An EMenuClass type to which this factory applies.
 * @menuid: The identifier of the menu for this factory, or NULL to be
 * called on all menus.
 * @func: An EMenuFactoryFunc callback.
 * @data: Callback data for @func.
 * 
 * Add a menu factory which will be called when the menu @menuid is
 * created.  The factory is free to add new items as it wishes to the
 * menu provided in the callback.
 *
 * TODO: Make the menuid a pattern?
 * 
 * Return value: A handle to the factory.
 **/
EMenuFactory *
e_menu_class_add_factory(EMenuClass *klass, const char *menuid, EMenuFactoryFunc func, void *data)
{
	struct _EMenuFactory *f = g_malloc0(sizeof(*f));

	d(printf("%p adding factory '%s' to class '%s'\n", klass, menuid?menuid:"<all menus>", g_type_name(((GObjectClass *)klass)->g_type_class.g_type)));

	f->menuid = g_strdup(menuid);
	f->factory = func;
	f->factory_data = data;
	e_dlist_addtail(&klass->factories, (EDListNode *)f);

	/* setup the menu itself based on factories */
	{
		struct _EMenuFactory *j;

		j = (struct _EMenuFactory *)klass->factories.head;
		if (j->next == NULL) {
			d(printf("%p no factories registered on menu???\n", klass));
		}
	}

	return f;
}

/**
 * e_menu_class_remove_factory:
 * @klass: Class on which the factory was originally added.
 * @f: Factory handle.
 * 
 * Remove a popup factory.  This must only be called once, and must
 * only be called using a valid factory handle @f.  After this call,
 * @f is undefined.
 **/
void
e_menu_class_remove_factory(EMenuClass *klass, EMenuFactory *f)
{
	e_dlist_remove((EDListNode *)f);
	g_free(f->menuid);
	g_free(f);
}

/**
 * e_menu_target_new:
 * @ep: An EMenu to which this target applies.
 * @type: Target type, up to implementation.
 * @size: Size of memory to allocate.  Must be >= sizeof(EMenuTarget).
 * 
 * Allocate a new menu target suitable for this class.  @size is used
 * to specify the actual target size, which may vary depending on the
 * implementing class.
 **/
void *e_menu_target_new(EMenu *ep, int type, size_t size)
{
	EMenuTarget *t;

	g_assert(size >= sizeof(EMenuTarget));

	t = g_malloc0(size);
	t->menu = ep;
	g_object_ref(ep);
	t->type = type;

	return t;
}

/**
 * e_menu_target_free:
 * @ep: EMenu on which the target was allocated.
 * @o: Tareget to free.
 * 
 * Free a target.
 **/
void
e_menu_target_free(EMenu *ep, void *o)
{
	EMenuTarget *t = o;

	((EMenuClass *)G_OBJECT_GET_CLASS(ep))->target_free(ep, t);
}

/* ********************************************************************** */

/* Main menu plugin handler */

/* NB: This has significant overlap with EPopupHook */

/*
<e-plugin
  class="org.gnome.mail.plugin.popup:1.0"
  id="org.gnome.mail.plugin.popup.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="Main menu plugin">
  <hook class="org.gnome.evolution.bonobomenu:1.0">
  <menu id="any" target="select" view="org.gnome.mail">
   <ui file="ui file1"/>
   <ui file="ui file2"/>
   <pixmap command="command" pixmap="stockname" size="menu|button|small_toolbar|large_toolbar|dnd|dialog"/>
   <item
    type="item|toggle"
    verb="verb"
    enable="select_one"
    visible="select_one"
    activate="doactivate"/>
   </menu>
  </hook>
  </extension>

*/

static void *emph_parent_class;
#define emph ((EMenuHook *)eph)

/* must have 1:1 correspondence with e-menu types in order */
static const EPluginHookTargetKey emph_item_types[] = {
	{ "item", E_MENU_ITEM },
	{ "toggle", E_MENU_TOGGLE },
	{ "radio", E_MENU_RADIO },
	{ 0 }
};

/* 1:1 with e-icon-factory sizes */
static const EPluginHookTargetKey emph_pixmap_sizes[] = {
	{ "menu", 0 },
	{ "button", 1},
	{ "small_toolbar", 2},
	{ "large_toolbar", 3},
	{ "dnd", 4},
	{ "dialog", 5},
	{ 0 }
};

static void
emph_menu_activate(EMenu *em, EMenuItem *item, void *data)
{
	EMenuHook *hook = data;

	d(printf("invoking plugin hook '%s' %p\n", (char *)item->user_data, em->target));

	e_plugin_invoke(hook->hook.plugin, item->user_data, em->target);
}

static void
emph_menu_toggle_activate(EMenu *em, EMenuItem *item, int state, void *data)
{
	EMenuHook *hook = data;

	/* FIXME: where does the toggle state go? */
	d(printf("invoking plugin hook '%s' %p\n", (char *)item->user_data, em->target));

	e_plugin_invoke(hook->hook.plugin, item->user_data, em->target);
}

static void
emph_menu_factory(EMenu *emp, void *data)
{
	struct _EMenuHookMenu *menu = data;

	d(printf("menu factory, adding %d items\n", g_slist_length(menu->items)));

	if (menu->items)
		e_menu_add_items(emp, menu->items, menu->uis, menu->pixmaps, NULL, menu->hook);
}

static void
emph_free_item(struct _EMenuItem *item)
{
	g_free(item->path);
	g_free(item->verb);
	g_free(item->user_data);
	g_free(item);
}

static void
emph_free_ui(struct _EMenuUIFile *ui)
{
	g_free(ui->appdir);
	g_free(ui->appname);
	g_free(ui->filename);
}

static void
emph_free_pixmap(struct _EMenuPixmap *pixmap)
{
	g_free(pixmap->command);
	g_free(pixmap->name);
	g_free(pixmap->pixmap);
	g_free(pixmap);
}

static void
emph_free_menu(struct _EMenuHookMenu *menu)
{
	g_slist_foreach(menu->items, (GFunc)emph_free_item, NULL);
	g_slist_free(menu->items);
	g_slist_foreach(menu->uis, (GFunc)emph_free_ui, NULL);
	g_slist_free(menu->uis);
	g_slist_foreach(menu->pixmaps, (GFunc)emph_free_pixmap, NULL);
	g_slist_free(menu->pixmaps);

	g_free(menu->id);
	g_free(menu);
}

static struct _EMenuItem *
emph_construct_item(EPluginHook *eph, EMenuHookMenu *menu, xmlNodePtr root, EMenuHookTargetMap *map)
{
	struct _EMenuItem *item;

	d(printf("  loading menu item\n"));
	item = g_malloc0(sizeof(*item));
	item->type = e_plugin_hook_id(root, emph_item_types, "type");
	item->path = e_plugin_xml_prop(root, "path");
	item->verb = e_plugin_xml_prop(root, "verb");
	item->visible = e_plugin_hook_mask(root, map->mask_bits, "visible");
	item->enable = e_plugin_hook_mask(root, map->mask_bits, "enable");
	item->user_data = e_plugin_xml_prop(root, "activate");
	if ((item->type & E_MENU_TYPE_MASK) == E_MENU_TOGGLE)
		item->activate = G_CALLBACK(emph_menu_toggle_activate);
	else
		item->activate = G_CALLBACK(emph_menu_activate);

	if (item->type == -1 || item->user_data == NULL)
		goto error;

	d(printf("   path=%s\n", item->path));
	d(printf("   verb=%s\n", item->verb));

	return item;
error:
	d(printf("error!\n"));
	emph_free_item(item);
	return NULL;
}

static struct _EMenuPixmap *
emph_construct_pixmap(EPluginHook *eph, EMenuHookMenu *menu, xmlNodePtr root)
{
	struct _EMenuPixmap *pixmap;

	d(printf("  loading menu pixmap\n"));
	pixmap = g_malloc0(sizeof(*pixmap));
	pixmap->command = e_plugin_xml_prop(root, "command");
	pixmap->name = e_plugin_xml_prop(root, "pixmap");
	pixmap->size = e_plugin_hook_id(root, emph_pixmap_sizes, "size");

	if (pixmap->command == NULL || pixmap->name == NULL || pixmap->size == -1)
		goto error;

	return pixmap;
error:
	d(printf("error!\n"));
	emph_free_pixmap(pixmap);
	return NULL;
}

static struct _EMenuHookMenu *
emph_construct_menu(EPluginHook *eph, xmlNodePtr root)
{
	struct _EMenuHookMenu *menu;
	xmlNodePtr node;
	EMenuHookTargetMap *map;
	EMenuHookClass *klass = (EMenuHookClass *)G_OBJECT_GET_CLASS(eph);
	char *tmp;

	d(printf(" loading menu\n"));
	menu = g_malloc0(sizeof(*menu));
	menu->hook = (EMenuHook *)eph;

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
			struct _EMenuItem *item;

			item = emph_construct_item(eph, menu, node, map);
			if (item)
				menu->items = g_slist_append(menu->items, item);
		} else if (0 == strcmp(node->name, "ui")) {
			tmp = xmlGetProp(node, "file");
			if (tmp) {
				EMenuUIFile *ui = g_malloc0(sizeof(*ui));

				ui->filename = tmp;
				ui->appdir = g_strdup("/tmp");
				ui->appname = g_strdup("Evolution");
				menu->uis = g_slist_append(menu->uis, ui);
			}
		} else if (0 == strcmp(node->name, "pixmap")) {
			struct _EMenuPixmap *pixmap;

			pixmap = emph_construct_pixmap(eph, menu, node);
			if (pixmap)
				menu->pixmaps = g_slist_append(menu->pixmaps, pixmap);
		}
		node = node->next;
	}

	return menu;
error:
	d(printf("error loading menu hook\n"));
	emph_free_menu(menu);
	return NULL;
}

static int
emph_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;
	EMenuClass *klass;

	d(printf("loading menu hook\n"));

	if (((EPluginHookClass *)emph_parent_class)->construct(eph, ep, root) == -1)
		return -1;

	klass = ((EMenuHookClass *)G_OBJECT_GET_CLASS(eph))->menu_class;

	node = root->children;
	while (node) {
		if (strcmp(node->name, "menu") == 0) {
			struct _EMenuHookMenu *menu;

			menu = emph_construct_menu(eph, node);
			if (menu) {
				printf(" plugin adding factory %p\n", klass);
				e_menu_class_add_factory(klass, menu->id, emph_menu_factory, menu);
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
	printf("EMenuHook class init %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type));

	((GObjectClass *)klass)->finalize = emph_finalise;
	klass->construct = emph_construct;

	/* this is actually an abstract implementation but list it anyway */
	klass->id = "org.gnome.evolution.bonobomenu:1.0";

	((EMenuHookClass *)klass)->target_map = g_hash_table_new(g_str_hash, g_str_equal);
	((EMenuHookClass *)klass)->menu_class = g_type_class_ref(e_menu_get_type());
}

/**
 * e_menu_hook_get_type:
 * 
 * Standard GObject function to get the object type.  Used to subclass
 * EMenuHook.
 * 
 * Return value: The type of the menu hook class.
 **/
GType
e_menu_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMenuHookClass), NULL, NULL, (GClassInitFunc) emph_class_init, NULL, NULL,
			sizeof(EMenuHook), 0, (GInstanceInitFunc) NULL,
		};

		emph_parent_class = g_type_class_ref(e_plugin_hook_get_type());
		type = g_type_register_static(e_plugin_hook_get_type(), "EMenuHook", &info, 0);
	}
	
	return type;
}

/**
 * e_menu_hook_class_add_target_map:
 * @klass: The derived EMenuHook class.
 * @map: A map used to describe a single EMenuTarget for this class.
 * 
 * Adds a target map to a concrete derived class of EMenu.  The target
 * map enumerates a single target type, and the enable mask bit names,
 * so that the type can be loaded automatically by the EMenu class.
 **/
void e_menu_hook_class_add_target_map(EMenuHookClass *klass, const EMenuHookTargetMap *map)
{
	g_hash_table_insert(klass->target_map, (void *)map->type, (void *)map);
}
