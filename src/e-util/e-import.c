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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#ifdef HAVE_IMPORT_H
#include <import.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "e-import.h"

#include <glib/gi18n.h>

#define d(x)

typedef struct _EImportImporters EImportImporters;

struct _EImportImporters {
	EImportImporter *importer;
	EImportImporterFunc free;
	gpointer data;
};

G_DEFINE_TYPE (
	EImport,
	e_import,
	G_TYPE_OBJECT)

static void
import_finalize (GObject *object)
{
	EImport *import = E_IMPORT (object);

	g_free (import->id);

	/* Chain up to parent's finalize () method. */
	G_OBJECT_CLASS (e_import_parent_class)->finalize (object);
}

static void
import_target_free (EImport *import,
                    EImportTarget *target)
{
	switch (target->type) {
	case E_IMPORT_TARGET_URI: {
		EImportTargetURI *s = (EImportTargetURI *) target;

		g_free (s->uri_src);
		g_free (s->uri_dest);
		break; }
	default:
		break;
	}

	g_datalist_clear (&target->data);
	g_free (target);
	g_object_unref (import);
}

static void
e_import_class_init (EImportClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = import_finalize;

	class->target_free = import_target_free;
}

static void
e_import_init (EImport *import)
{
}

/**
 * e_import_construct:
 * @import: The instance to initialise.
 * @id: The name of the instance.
 *
 * Used by implementing classes to initialise base parameters.
 *
 * Return value: @ep is returned.
 **/
EImport *
e_import_construct (EImport *import,
                    const gchar *id)
{
	import->id = g_strdup (id);

	return import;
}

EImport *
e_import_new (const gchar *id)
{
	EImport *import;

	import = g_object_new (E_TYPE_IMPORT, NULL);

	return e_import_construct (import, id);
}

/**
 * e_import_import:
 * @import: an #EImport
 * @target: Target to import.
 * @importer: Importer to use.
 * @status: Status callback, called with progress information.
 * @done: Complete callback, will always be called once complete.
 * @data: user data for callback functions
 *
 * Run the import function of the selected importer.  Once the
 * importer has finished, it MUST call the e_import_complete ()
 * function.  This allows importers to run in synchronous or
 * asynchronous mode.
 *
 * When complete, the @done callback will be called.
 **/
void
e_import_import (EImport *import,
                 EImportTarget *target,
                 EImportImporter *importer,
                 EImportStatusFunc status,
                 EImportCompleteFunc done,
                 gpointer data)
{
	g_return_if_fail (importer != NULL);

	import->status = status;
	import->done = done;
	import->done_data = data;

	importer->import (import, target, importer);
}

void
e_import_cancel (EImport *import,
                 EImportTarget *t,
                 EImportImporter *im)
{
	if (im->cancel)
		im->cancel (import, t, im);
}

/**
 * e_import_get_widget:
 * @import: an #EImport
 * @target: Target of interest
 * @importer: Importer to get widget of
 *
 * Gets a widget that the importer uses to configure its
 * destination.  This widget should be packed into a container
 * widget.  It should not be shown_all.
 *
 * Return value: NULL if the importer doesn't support/require
 * a destination.
 **/
GtkWidget *
e_import_get_widget (EImport *import,
                     EImportTarget *target,
                     EImportImporter *importer)
{
	g_return_val_if_fail (importer != NULL, NULL);
	g_return_val_if_fail (target != NULL, NULL);

	return importer->get_widget (import, target, importer);
}

/**
 * e_import_get_preview_widget:
 * @import: an #EImport
 * @target: Target of interest
 * @im: Importer to get a preview widget of
 *
 * Gets a widget that the importer uses to preview data to be
 * imported.  This widget should be packed into a container
 * widget.  It should not be shown_all.
 *
 * Return value: NULL if the importer doesn't support preview.
 **/
GtkWidget *
e_import_get_preview_widget (EImport *import,
                     EImportTarget *target,
                     EImportImporter *im)
{
	g_return_val_if_fail (im != NULL, NULL);
	g_return_val_if_fail (target != NULL, NULL);

	if (!im->get_preview)
		return NULL;

	return im->get_preview (import, target, im);
}

/**
 * e_import_complete:
 * @import: an #EImport
 * @target: Target just completed (unused currently)
 * @error: a #GError for the operation, %NULL when succeeded
 *
 * Signify that an import is complete.  This must be called by
 * importer implementations when they are done.
 **/
void
e_import_complete (EImport *import,
                   EImportTarget *target,
		   const GError *error)
{
	if (import->done)
		import->done (import, error, import->done_data);
}

void
e_import_status (EImport *import,
                 EImportTarget *target,
                 const gchar *what,
                 gint pc)
{
	if (import->status)
		import->status (import, what, pc, import->done_data);
}

/**
 * e_import_get_importers:
 * @import: an #EImport
 * @target: an #EImportTarget
 *
 * Get a list of importers.  If @target is supplied, then only
 * importers which support the type and location specified by the
 * target are listed.  If @target is NULL, then all importers are
 * listed.
 *
 * Return value: A list of importers.  The list should be freed when
 * no longer needed.
 **/
GSList *
e_import_get_importers (EImport *import,
                        EImportTarget *target)
{
	GSList *importers = NULL;
	GList *link;

	link = E_IMPORT_GET_CLASS (import)->importers;

	while (link != NULL) {
		EImportImporters *ei = link->data;

		if (target == NULL
		    || (ei->importer->type == target->type
			&& ei->importer->supported (import, target, ei->importer))) {
			importers = g_slist_append (importers, ei->importer);
		}

		link = g_list_next (link);
	}

	return importers;
}

/* ********************************************************************** */

static gint
importer_compare (EImportImporters *node_a,
                  EImportImporters *node_b)
{
	gint pri_a = node_a->importer->pri;
	gint pri_b = node_b->importer->pri;

	return (pri_a == pri_b) ? 0 : (pri_a < pri_b) ? -1 : 1;
}

/**
 * e_import_class_add_importer:
 * @klass: An initialised implementing instance of EImport.
 * @importer: Importer to add.
 * @freefunc: If supplied, called to free the importer node
 * when it is no longer needed.
 * @data: Data for the callback.
 *
 **/
void
e_import_class_add_importer (EImportClass *klass,
                             EImportImporter *importer,
                             EImportImporterFunc freefunc,
                             gpointer data)
{
	EImportImporters *node;

	node = g_malloc (sizeof (*node));
	node->importer = importer;
	node->free = freefunc;
	node->data = data;

	klass->importers = g_list_sort (
		g_list_prepend (klass->importers, node),
		(GCompareFunc) importer_compare);
}

/**
 * e_import_target_new:
 * @import: an #EImport
 * @type: type, up to implementor
 * @size: Size of object to allocate.
 *
 * Allocate a new import target suitable for this class.  Implementing
 * classes will define the actual content of the target.
 **/
gpointer
e_import_target_new (EImport *import,
                     gint type,
                     gsize size)
{
	EImportTarget *target;

	if (size < sizeof (EImportTarget)) {
		g_warning ("Size less than size of EImportTarget\n");
		size = sizeof (EImportTarget);
	}

	target = g_malloc0 (size);
	target->import = g_object_ref (import);
	target->type = type;

	g_datalist_init (&target->data);

	return target;
}

/**
 * e_import_target_free:
 * @import: an #EImport
 * @target: the target to free
 *
 * Free a target.  The implementing class can override this method to
 * free custom targets.
 **/
void
e_import_target_free (EImport *import,
                      gpointer target)
{
	E_IMPORT_GET_CLASS (import)->target_free (
		import, (EImportTarget *) target);
}

EImportTargetURI *
e_import_target_new_uri (EImport *import,
                         const gchar *uri_src,
                         const gchar *uri_dst)
{
	EImportTargetURI *t;

	t = e_import_target_new (import, E_IMPORT_TARGET_URI, sizeof (*t));
	t->uri_src = g_strdup (uri_src);
	t->uri_dest = g_strdup (uri_dst);

	return t;
}

EImportTargetHome *
e_import_target_new_home (EImport *import)
{
	return e_import_target_new (
		import, E_IMPORT_TARGET_HOME, sizeof (EImportTargetHome));
}

/* ********************************************************************** */

/* Import menu plugin handler */

/*
 * <e-plugin
 *   class="org.gnome.mail.plugin.import:1.0"
 *   id="org.gnome.mail.plugin.import.item:1.0"
 *   type="shlib"
 *   location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
 *   name="imap"
 *   description="IMAP4 and IMAP4v1 mail store">
 *   <hook class="org.gnome.mail.importMenu:1.0"
 *         handler="HandleImport">
 *   <menu id="any" target="select">
 *    <item
 *     type="item|toggle|radio|image|submenu|bar"
 *     active
 *     path="foo/bar"
 *     label="label"
 *     icon="foo"
 *     activate="ep_view_emacs"/>
 *   </menu>
 * </e-plugin>
 */

#define emph ((EImportHook *)eph)

static const EImportHookTargetMask eih_no_masks[] = {
	{ NULL }
};

static const EImportHookTargetMap eih_targets[] = {
	{ "uri", E_IMPORT_TARGET_URI, eih_no_masks },
	{ "home", E_IMPORT_TARGET_HOME, eih_no_masks },
	{ NULL }
};

G_DEFINE_TYPE (
	EImportHook,
	e_import_hook,
	E_TYPE_PLUGIN_HOOK)

static gboolean
eih_supported (EImport *ei,
               EImportTarget *target,
               EImportImporter *im)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *) im;
	EImportHook *hook = im->user_data;

	return e_plugin_invoke (hook->hook.plugin, ihook->supported, target) != NULL;
}

static GtkWidget *
eih_get_widget (EImport *ei,
                EImportTarget *target,
                EImportImporter *im)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *) im;
	EImportHook *hook = im->user_data;

	return e_plugin_invoke (hook->hook.plugin, ihook->get_widget, target);
}

static void
eih_import (EImport *ei,
            EImportTarget *target,
            EImportImporter *im)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *) im;
	EImportHook *hook = im->user_data;

	e_plugin_invoke (hook->hook.plugin, ihook->import, target);
}

static void
eih_cancel (EImport *ei,
            EImportTarget *target,
            EImportImporter *im)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *) im;
	EImportHook *hook = im->user_data;

	e_plugin_invoke (hook->hook.plugin, ihook->cancel, target);
}

static void
eih_free_importer (EImportImporter *im,
                   gpointer data)
{
	EImportHookImporter *ihook = (EImportHookImporter *) im;

	g_free (ihook->supported);
	g_free (ihook->get_widget);
	g_free (ihook->import);
	g_free (ihook);
}

static struct _EImportHookImporter *
emph_construct_importer (EPluginHook *eph,
                         xmlNodePtr root)
{
	struct _EImportHookImporter *item;
	EImportHookTargetMap *map;
	EImportHookClass *class = (EImportHookClass *) G_OBJECT_GET_CLASS (eph);
	gchar *tmp;

	d (printf ("  loading import item\n"));
	item = g_malloc0 (sizeof (*item));

	tmp = (gchar *) xmlGetProp (root, (const guchar *)"target");
	if (tmp == NULL)
		goto error;
	map = g_hash_table_lookup (class->target_map, tmp);
	xmlFree (tmp);
	if (map == NULL)
		goto error;

	item->importer.type = map->id;
	item->supported = e_plugin_xml_prop (root, "supported");
	item->get_widget = e_plugin_xml_prop (root, "get-widget");
	item->import = e_plugin_xml_prop (root, "import");
	item->cancel = e_plugin_xml_prop (root, "cancel");

	item->importer.name = e_plugin_xml_prop (root, "name");
	item->importer.description = e_plugin_xml_prop (root, "description");

	item->importer.user_data = eph;

	if (item->import == NULL || item->supported == NULL)
		goto error;

	item->importer.supported = eih_supported;
	item->importer.import = eih_import;
	if (item->get_widget)
		item->importer.get_widget = eih_get_widget;
	if (item->cancel)
		item->importer.cancel = eih_cancel;

	return item;
error:
	d (printf ("error!\n"));
	eih_free_importer ((EImportImporter *) item, NULL);
	return NULL;
}

static gint
emph_construct (EPluginHook *eph,
                EPlugin *ep,
                xmlNodePtr root)
{
	xmlNodePtr node;
	EImportClass *class;

	d (printf ("loading import hook\n"));

	if (E_PLUGIN_HOOK_CLASS (e_import_hook_parent_class)->
		construct (eph, ep, root) == -1)
		return -1;

	class = E_IMPORT_HOOK_GET_CLASS (eph)->import_class;

	node = root->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "importer") == 0) {
			struct _EImportHookImporter *ihook;

			ihook = emph_construct_importer (eph, node);
			if (ihook) {
				e_import_class_add_importer (
					class, &ihook->importer,
					eih_free_importer, eph);
				emph->importers = g_slist_append (
					emph->importers, ihook);
			}
		}
		node = node->next;
	}

	eph->plugin = ep;

	return 0;
}

static void
e_import_hook_class_init (EImportHookClass *class)
{
	EPluginHookClass *plugin_hook_class;
	gint ii;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = "org.gnome.evolution.import:1.0";
	plugin_hook_class->construct = emph_construct;

	/** @HookClass: Evolution Importers
	 * @Id: org.gnome.evolution.import:1.0
	 * @Target: EImportTarget
	 *
	 * A hook for data importers.
	 **/

	class->target_map = g_hash_table_new (g_str_hash, g_str_equal);
	class->import_class = g_type_class_ref (E_TYPE_IMPORT);

	for (ii = 0; eih_targets[ii].type; ii++)
		e_import_hook_class_add_target_map (class, &eih_targets[ii]);
}

static void
e_import_hook_init (EImportHook *hook)
{
}

/**
 * e_import_hook_class_add_target_map:
 *
 * @class: The dervied EimportHook class.
 * @map: A map used to describe a single EImportTarget type for this
 * class.
 *
 * Add a targe tmap to a concrete derived class of EImport.  The
 * target map enumates the target types available for the implenting
 * class.
 **/
void
e_import_hook_class_add_target_map (EImportHookClass *class,
                                    const EImportHookTargetMap *map)
{
	g_hash_table_insert (
		class->target_map, (gpointer) map->type, (gpointer) map);
}
