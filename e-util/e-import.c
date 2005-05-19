/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2005 Novell Inc.
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

#ifdef HAVE_IMPORT_H
#include <import.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include <gtk/gtknotebook.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkalignment.h>

#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page-standard.h>
#include <libgnomeui/gnome-druid-page-edge.h>

#include "e-import.h"

#include <e-util/e-icon-factory.h>

#include <libgnome/gnome-i18n.h>

#define d(x) 

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, e_import_get_type()))

struct _EImportImporters {
	struct _EImportImporters *next, *prev;

	EImportImporter *importer;
	EImportImporterFunc free;
	void *data;
};

struct _EImportPrivate {
	int dummy;
};

static GObjectClass *ep_parent;

static void
ep_init(GObject *o)
{
	/*EImport *emp = (EImport *)o;*/
	struct _EImportPrivate *p;

	p = _PRIVATE(o);
}

static void
ep_finalise(GObject *o)
{
	EImport *emp = (EImport *)o;
	struct _EImportPrivate *p;

	p = _PRIVATE(emp);

	d(printf("finalising EImport %p\n", o));

	g_free(emp->id);

	((GObjectClass *)ep_parent)->finalize(o);
}

static void
ec_target_free(EImport *ep, EImportTarget *t)
{
	switch (t->type) {
	case E_IMPORT_TARGET_URI: {
		EImportTargetURI *s = (EImportTargetURI *)t;

		g_free(s->uri_src);
		g_free(s->uri_dest);
		break; }
	case E_IMPORT_TARGET_HOME: {
		EImportTargetHome *s = (EImportTargetHome *)t;

		g_free(s->homedir);
		break; }
	}

	g_free(t);
	g_object_unref(ep);
}

static void
ec_set_target(EImport *emp, EImportTarget *target)
{
	EImportClass *k = (EImportClass *)G_OBJECT_GET_CLASS(emp);
	struct _EImportImporters *ei;

	if (emp->target)
		e_import_target_free(emp, target);

	emp->target = target;
	emp->importer = NULL;

	if (target== NULL)
		return;

	for (ei = (struct _EImportImporters *)k->importers.head;
	     ei->next;
	     ei = ei->next) {
		if (ei->importer->type == target->type
		    && ei->importer->supported(emp, ei->importer, ei->importer->user_data)) {
			emp->importer = ei->importer;
			break;
		}
	}
}

static void
ep_class_init(GObjectClass *klass)
{
	d(printf("EImport class init %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type)));

	g_type_class_add_private(klass, sizeof(struct _EImportPrivate));

	klass->finalize = ep_finalise;
	((EImportClass *)klass)->set_target = ec_set_target;
	((EImportClass *)klass)->target_free = ec_target_free;
}

static void
ep_base_init(GObjectClass *klass)
{
	e_dlist_init(&((EImportClass *)klass)->importers);
}

/**
 * e_import_get_type:
 * 
 * Standard GObject method.  Used to subclass for the concrete
 * implementations.
 * 
 * Return value: EImport type.
 **/
GType
e_import_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EImportClass),
			(GBaseInitFunc)ep_base_init, NULL,
			(GClassInitFunc)ep_class_init, NULL, NULL,
			sizeof(EImport), 0,
			(GInstanceInitFunc)ep_init
		};
		ep_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EImport", &info, 0);
	}

	return type;
}

/**
 * e_import_construct:
 * @ep: The instance to initialise.
 * @id: The name of the instance.
 * 
 * Used by implementing classes to initialise base parameters.
 * 
 * Return value: @ep is returned.
 **/
EImport *e_import_construct(EImport *ep, const char *id)
{
	ep->id = g_strdup(id);

	return ep;
}

/**
 * e_import_import:
 * @ei: 
 * @done:
 * @data:
 * 
 * Run the import function of the selected importer.  Once the
 * importer has finished, it MUST call the e_import_complete()
 * function.  This allows importers to run in synchronous or
 * asynchronous mode.
 *
 * When complete, the @done callback will be called.
 **/
void
e_import_import(EImport *ei, EImportCompleteFunc done, void *data)
{
	g_return_if_fail(ei->importer != NULL);
	g_return_if_fail(ei->target != NULL);

	ei->done = done;
	ei->done_data = data;

	ei->importer->import(ei, ei->importer, ei->importer->user_data);
}

/**
 * e_import_get_widget:
 * @ei: An import object on which the target has been set.
 * 
 * Gets a widget that the importer uses to configure its
 * destination.  This widget should be packed into a container
 * widget.
 * 
 * Return value: NULL if the importer doesn't support/require
 * a destination.
 **/
struct _GtkWidget *
e_import_get_widget(EImport *ei)
{
	g_return_val_if_fail(ei->importer != NULL, NULL);
	g_return_val_if_fail(ei->target != NULL, NULL);

	return ei->importer->get_widget(ei, ei->importer, ei->importer->user_data);
}

/**
 * e_import_complete:
 * @ei: 
 * 
 * Signify that an import is complete.  This must be called by
 * importer implementations when they are done.
 **/
void e_import_complete(EImport *ei)
{
	if (ei->done)
		ei->done(ei, ei->done_data);
}

/**
 * e_import_set_target:
 * @emp: An initialised EImport.
 * @target: A target allocated from @emp.
 * 
 * Sets the target object for the import window.  Generally the target
 * is set only once, and will supply its own "changed" signal which
 * can be used to drive the modal.  This is a virtual method so that
 * the implementing class can connect to the changed signal and
 * initiate a e_import_target_changed() call where appropriate.
 **/
void
e_import_set_target(EImport *emp, EImportTarget *target)
{
	if (emp->target != target)
		((EImportClass *)G_OBJECT_GET_CLASS(emp))->set_target(emp, target);
}

/**
 * e_import_get_importers:
 * @emp: 
 * @target: 
 * 
 * Get a list of importers.  If @target is supplied, then only
 * importers which support the location specified by the target are
 * listed.  If @target is NULL, then all importers are listed.
 * 
 * Return value: A list of importers.  The list should be freed when
 * no longer needed.
 **/
GSList *
e_import_get_importers(EImport *emp, EImportTarget *target)
{
	EImportClass *k = (EImportClass *)G_OBJECT_GET_CLASS(emp);
	struct _EImportImporters *ei;
	GSList *importers = NULL;

	for (ei = (struct _EImportImporters *)k->importers.head;
	     ei->next;
	     ei = ei->next) {
		if (target == NULL
		    || (ei->importer->type == target->type
			&& ei->importer->supported(emp, ei->importer, ei->importer->user_data))) {
			importers = g_slist_append(importers, ei->importer);
			break;
		}
	}

	return importers;
}

/* ********************************************************************** */

/**
 * e_import_class_add_importer:
 * @ec: An initialised implementing instance of EImport.
 * @importer: Importer to add.
 * @freefunc: If supplied, called to free the importer node
 * when it is no longer needed.
 * @data: Data for the callback.
 * 
 **/
void
e_import_class_add_importer(EImportClass *klass, EImportImporter *importer, EImportImporterFunc freefunc, void *data)
{
	struct _EImportImporters *node, *ei, *en;

	node = g_malloc(sizeof(*node));
	node->importer = importer;
	node->free = freefunc;
	node->data = data;
	ei = (struct _EImportImporters *)klass->importers.head;
	en = ei->next;
	while (en && ei->importer->pri < importer->pri) {
		ei = en;
		en = en->next;
	}

	node->next = ei->next;
	node->next->prev = node;
	node->prev = ei;
	ei->next = node;
}

void e_import_class_remove_importer(EImportClass *klass, EImportImporter *f)
{
	struct _EImportImporters *ei, *en;

	ei = (struct _EImportImporters *)klass->importers.head;
	en = ei->next;
	while (en) {
		if (ei->importer == f) {
			e_dlist_remove((EDListNode *)ei);
			if (ei->free)
				ei->free(f, ei->data);
			g_free(ei);
		}
		ei = en;
		en = en->next;
	}
}

/**
 * e_import_target_new:
 * @ep: Parent EImport object.
 * @type: type, up to implementor
 * @size: Size of object to allocate.
 * 
 * Allocate a new import target suitable for this class.  Implementing
 * classes will define the actual content of the target.
 **/
void *e_import_target_new(EImport *ep, int type, size_t size)
{
	EImportTarget *t;

	g_assert(size >= sizeof(EImportTarget));

	t = g_malloc0(size);
	t->import = ep;
	g_object_ref(ep);
	t->type = type;

	return t;
}

/**
 * e_import_target_free:
 * @ep: Parent EImport object.
 * @o: The target to fre.
 * 
 * Free a target.  The implementing class can override this method to
 * free custom targets.
 **/
void
e_import_target_free(EImport *ep, void *o)
{
	EImportTarget *t = o;

	((EImportClass *)G_OBJECT_GET_CLASS(ep))->target_free(ep, t);
}

EImportTargetURI *e_import_target_new_uri(EImport *ei, const char *suri, const char *duri)
{
	EImportTargetURI *t = e_import_target_new(ei, E_IMPORT_TARGET_URI, sizeof(*t));

	t->uri_src = g_strdup(suri);
	t->uri_dest = g_strdup(duri);

	return t;
}

EImportTargetHome *e_import_target_new_home(EImport *ei, const char *home)
{
	EImportTargetHome *t = e_import_target_new(ei, E_IMPORT_TARGET_HOME, sizeof(*t));

	t->homedir = g_strdup(home);

	return t;
}

/* ********************************************************************** */

/* Import menu plugin handler */

/*
<e-plugin
  class="org.gnome.mail.plugin.import:1.0"
  id="org.gnome.mail.plugin.import.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="org.gnome.mail.importMenu:1.0"
        handler="HandleImport">
  <menu id="any" target="select">
   <item
    type="item|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    activate="ep_view_emacs"/>
  </menu>
  </extension>

*/

static void *emph_parent_class;
#define emph ((EImportHook *)eph)

static const EImportHookTargetMask eih_no_masks[] = {
	{ 0 }
};

static const EImportHookTargetMap eih_targets[] = {
	{ "uri", E_IMPORT_TARGET_URI, eih_no_masks },
	{ "home", E_IMPORT_TARGET_URI, eih_no_masks },
	{ 0 }
};

static gboolean eih_supported(EImport *ei, EImportImporter *im, void *data)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *)im;
	EImportHook *hook = im->user_data;

	return e_plugin_invoke(hook->hook.plugin, ihook->supported, ei) != NULL;
}

static struct _GtkWidget *eih_get_widget(EImport *ei, EImportImporter *im, void *data)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *)im;
	EImportHook *hook = im->user_data;

	return e_plugin_invoke(hook->hook.plugin, ihook->get_widget, ei);
}

static void eih_import(EImport *ei, EImportImporter *im, void *data)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *)im;
	EImportHook *hook = im->user_data;

	e_plugin_invoke(hook->hook.plugin, ihook->import, ei);
}

static void
eih_free_importer(EImportImporter *im, void *data)
{
	EImportHookImporter *ihook = (EImportHookImporter *)im;

	g_free(ihook->supported);
	g_free(ihook->get_widget);
	g_free(ihook->import);
	g_free(ihook);
}

static struct _EImportHookImporter *
emph_construct_importer(EPluginHook *eph, xmlNodePtr root)
{
	struct _EImportHookImporter *item;
	EImportHookTargetMap *map;
	EImportHookClass *klass = (EImportHookClass *)G_OBJECT_GET_CLASS(eph);
	char *tmp;

	d(printf("  loading import item\n"));
	item = g_malloc0(sizeof(*item));

	tmp = xmlGetProp(root, "target");
	if (tmp == NULL)
		goto error;
	map = g_hash_table_lookup(klass->target_map, tmp);
	xmlFree(tmp);
	if (map == NULL)
		goto error;

	item->importer.type = map->id;
	item->supported = e_plugin_xml_prop(root, "supported");
	item->get_widget = e_plugin_xml_prop(root, "get-widget");
	item->import = e_plugin_xml_prop(root, "import");

	item->importer.name = e_plugin_xml_prop(root, "name");
	item->importer.description = e_plugin_xml_prop(root, "description");

	item->importer.user_data = eph;

	if (item->import == NULL || item->supported == NULL)
		goto error;

	item->importer.supported = eih_supported;
	item->importer.import = eih_import;
	if (item->get_widget)
		item->importer.get_widget = eih_get_widget;

	return item;
error:
	d(printf("error!\n"));
	eih_free_importer((EImportImporter *)item, NULL);
	return NULL;
}

static int
emph_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;
	EImportClass *klass;

	d(printf("loading import hook\n"));

	if (((EPluginHookClass *)emph_parent_class)->construct(eph, ep, root) == -1)
		return -1;

	klass = ((EImportHookClass *)G_OBJECT_GET_CLASS(eph))->import_class;

	node = root->children;
	while (node) {
		if (strcmp(node->name, "importer") == 0) {
			struct _EImportHookImporter *ihook;

			ihook = emph_construct_importer(eph, node);
			if (ihook) {
				e_import_class_add_importer(klass, &ihook->importer, eih_free_importer, eph);
				emph->importers = g_slist_append(emph->importers, ihook);
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
	/*EPluginHook *eph = (EPluginHook *)o;*/

	/* free importers? */

	((GObjectClass *)emph_parent_class)->finalize(o);
}

static void
emph_class_init(EPluginHookClass *klass)
{
	int i;

	((GObjectClass *)klass)->finalize = emph_finalise;
	klass->construct = emph_construct;

	/* this is actually an abstract implementation but list it anyway */
	klass->id = "org.gnome.evolution.import:1.0";

	d(printf("EImportHook: init class %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type)));

	((EImportHookClass *)klass)->target_map = g_hash_table_new(g_str_hash, g_str_equal);
	((EImportHookClass *)klass)->import_class = g_type_class_ref(e_import_get_type());

	for (i=0;eih_targets[i].type;i++)
		e_import_hook_class_add_target_map((EImportHookClass *)klass, &eih_targets[i]);
}

/**
 * e_import_hook_get_type:
 * 
 * Standard GObject function to get the object type.
 * 
 * Return value: The EImportHook class type.
 **/
GType
e_import_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EImportHookClass), NULL, NULL, (GClassInitFunc) emph_class_init, NULL, NULL,
			sizeof(EImportHook), 0, (GInstanceInitFunc) NULL,
		};

		emph_parent_class = g_type_class_ref(e_plugin_hook_get_type());
		type = g_type_register_static(e_plugin_hook_get_type(), "EImportHook", &info, 0);
	}
	
	return type;
}

/**
 * e_import_hook_class_add_target_map:
 *
 * @klass: The dervied EimportHook class.
 * @map: A map used to describe a single EImportTarget type for this
 * class.
 * 
 * Add a targe tmap to a concrete derived class of EImport.  The
 * target map enumates the target types available for the implenting
 * class.
 **/
void e_import_hook_class_add_target_map(EImportHookClass *klass, const EImportHookTargetMap *map)
{
	g_hash_table_insert(klass->target_map, (void *)map->type, (void *)map);
}
