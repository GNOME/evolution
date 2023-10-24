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
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_IMPORT_H
#define E_IMPORT_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_IMPORT \
	(e_import_get_type ())
#define E_IMPORT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_IMPORT, EImport))
#define E_IMPORT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_IMPORT, EImportClass))
#define E_IS_IMPORT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_IMPORT))
#define E_IS_IMPORT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_IMPORT))
#define E_IMPORT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_IMPORT, EImportClass))

G_BEGIN_DECLS

typedef struct _EImport EImport;
typedef struct _EImportClass EImportClass;

typedef struct _EImportImporter EImportImporter;
typedef struct _EImportFactory EImportFactory;
typedef struct _EImportTarget EImportTarget;

typedef void		(*EImportCompleteFunc)	(EImport *ei,
						 const GError *error,
						 gpointer user_data);
typedef void		(*EImportStatusFunc)	(EImport *ei,
						 const gchar *what,
						 gint pc,
						 gpointer data);

typedef void		(*EImportFactoryFunc)	(EImport *ei,
						 gpointer data);
typedef void		(*EImportImporterFunc)	(EImportImporter *importer,
						 gpointer data);
typedef gboolean	(*EImportSupportedFunc)	(EImport *ei,
						 EImportTarget *,
						 EImportImporter *im);
typedef GtkWidget *	(*EImportWidgetFunc)	(EImport *ei,
						 EImportTarget *,
						 EImportImporter *im);
typedef void		(*EImportImportFunc)	(EImport *ei,
						 EImportTarget *,
						 EImportImporter *im);

/* The global target types, implementors may add additional ones */
enum _e_import_target_t {
	E_IMPORT_TARGET_URI,	/* simple file */
	E_IMPORT_TARGET_HOME,	/* a home-directory thing,
				 * i.e. old applications */
	E_IMPORT_TARGET_LAST = 256
};

/**
 * struct _EImportImporter -
 *
 * @type: target type
 * @priority: Priority of importer.  Higher values will be processed first.
 * @supported: Callback to see if this target is supported by the importer.
 * @get_widget: A widget factory for this importer, if it needs any extra
 * information in the assistant.  It will update the target.
 * @import: Run the import.
 * @cancel: Cancel the import.
 * @get_preview: Callback to create a preview widget for just importing data.
 * @user_data: User data for the callbacks;
 *
 * Base importer description.
 **/
struct _EImportImporter {
	enum _e_import_target_t type;

	gint pri;

	EImportSupportedFunc supported;
	EImportWidgetFunc get_widget;
	EImportImportFunc import;
	EImportImportFunc cancel;
	EImportWidgetFunc get_preview;

	gpointer user_data;

	/* ?? */
	gchar *name;
	gchar *description;
};

/**
 * struct _EImportTarget - importation context.
 *
 * @import: The parent object.
 * @type: The type of target, defined by implementing classes.
 * @data: This can be used to store run-time information
 * about this target.  Any allocated data must be set so
 * as to free it when the target is freed.
 *
 * The base target object is used as the parent and placeholder for
 * import context for a given importer.
 **/
struct _EImportTarget {
	EImport *import;

	guint32 type;

	GData *data;

	/* implementation fields follow, depends on target type */
};

typedef struct _EImportTargetURI EImportTargetURI;
typedef struct _EImportTargetHome EImportTargetHome;

struct _EImportTargetURI {
	EImportTarget target;

	gchar *uri_src;
	gchar *uri_dest;
};

struct _EImportTargetHome {
	EImportTarget target;
};

/**
 * struct _EImport - An importer management object.
 *
 * @object: Superclass.
 * @id: ID of importer.
 * @status: Status callback of current running import.
 * @done: Completion callback of current running import.
 * @done_data: Callback data for both of above.
 *
 **/
struct _EImport {
	GObject object;

	gchar *id;

	EImportStatusFunc status;
	EImportCompleteFunc done;
	gpointer done_data;
};

/**
 * struct _EImportClass - Importer manager abstract class.
 *
 * @object_class: Superclass.
 * @factories: A list of factories registered on this type of
 * importuration manager.
 * @set_target: A virtual method used to set the target on the
 * importuration manager.  This is used by subclasses so they may hook
 * into changes on the target to propery drive the manager.
 * @target_free: A virtual method used to free the target in an
 * implementation-defined way.
 *
 **/
struct _EImportClass {
	GObjectClass object_class;

	GList *importers;

	void		(*target_free)		(EImport *import,
						 EImportTarget *target);
};

GType		e_import_get_type		(void) G_GNUC_CONST;
EImport *	e_import_new			(const gchar *id);
void		e_import_class_add_importer	(EImportClass *klass,
						 EImportImporter *importer,
						 EImportImporterFunc freefunc,
						 gpointer data);
GSList *	e_import_get_importers		(EImport *import,
						 EImportTarget *target);
EImport *	e_import_construct		(EImport *import,
						 const gchar *id);
void		e_import_import			(EImport *import,
						 EImportTarget *target,
						 EImportImporter *importer,
						 EImportStatusFunc status,
						 EImportCompleteFunc done,
						 gpointer data);
void		e_import_cancel			(EImport *import,
						 EImportTarget *target,
						 EImportImporter *importer);
GtkWidget *	e_import_get_widget		(EImport *import,
						 EImportTarget *target,
						 EImportImporter *importer);
GtkWidget *	e_import_get_preview_widget	(EImport *import,
						 EImportTarget *target,
						 EImportImporter *im);
gboolean	e_import_get_widget_complete	(EImport *import);
void		e_import_set_widget_complete	(EImport *import,
						 gboolean value);
void		e_import_status			(EImport *import,
						 EImportTarget *target,
						 const gchar *what,
						 gint pc);
void		e_import_complete		(EImport *import,
						 EImportTarget *target,
						 const GError *error);
gpointer	e_import_target_new		(EImport *import,
						 gint type,
						 gsize size);
void		e_import_target_free		(EImport *import,
						 gpointer target);
EImportTargetURI *
		e_import_target_new_uri		(EImport *import,
						 const gchar *uri_src,
						 const gchar *uri_dst);
EImportTargetHome *
		e_import_target_new_home	(EImport *import);

gchar *		e_import_util_get_file_contents	(const gchar *filename,
						 gsize size_limit,
						 GError **error);

/* ********************************************************************** */

/* import plugin target, they are closely integrated */

/* To implement a basic import plugin, you just need to subclass
 * this and initialise the class target type tables */

#include <e-util/e-plugin.h>

/* Standard GObject macros */
#define E_TYPE_IMPORT_HOOK \
	(e_import_hook_get_type ())
#define E_IMPORT_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_IMPORT_HOOK, EImportHook))
#define E_IMPORT_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_IMPORT_HOOK, EImportHookClass))
#define E_IS_IMPORT_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_IMPORT_HOOK))
#define E_IS_IMPORT_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_IMPORT_HOOK))
#define E_IMPORT_HOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_IMPORT_HOOK, EImportHookClass))

typedef struct _EPluginHookTargetMap EImportHookTargetMap;
typedef struct _EPluginHookTargetKey EImportHookTargetMask;

typedef struct _EImportHook EImportHook;
typedef struct _EImportHookClass EImportHookClass;

typedef struct _EImportHookImporter EImportHookImporter;

struct _EImportHookImporter {
	EImportImporter importer;

	/* user_data == EImportHook */

	gchar *supported;
	gchar *get_widget;
	gchar *import;
	gchar *cancel;
};

/**
 * struct _EImportHook - Plugin hook for importuration windows.
 *
 * @hook: Superclass.
 * @groups: A list of EImportHookGroup's of all importuration windows
 * this plugin hooks into.
 *
 **/
struct _EImportHook {
	EPluginHook hook;

	GSList *importers;
};

/**
 * EImportHookClass:
 * @hook_class: Superclass.
 * @target_map: A table of EImportHookTargetMap structures describing
 * the possible target types supported by this class.
 * @import_class: The EImport derived class that this hook
 * implementation drives.
 *
 * This is an abstract class defining the plugin hook point for
 * import windows.
 *
 **/
struct _EImportHookClass {
	EPluginHookClass hook_class;

	/* EImportHookTargetMap by .type */
	GHashTable *target_map;
	/* the import class these imports's belong to */
	EImportClass *import_class;
};

GType		e_import_hook_get_type	(void);
void		e_import_hook_class_add_target_map
					(EImportHookClass *klass,
					 const EImportHookTargetMap *map);

G_END_DECLS

#endif /* E_IMPORT_H */
