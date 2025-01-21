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

#ifndef E_CONFIG_H
#define E_CONFIG_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_CONFIG \
	(e_config_get_type ())
#define E_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONFIG, EConfig))
#define E_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONFIG, EConfigClass))
#define E_IS_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONFIG))
#define E_IS_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONFIG))
#define E_CONFIG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONFIG, EConfigClass))

G_BEGIN_DECLS

/* This is a config window management/merging class. */

typedef struct _EConfig EConfig;
typedef struct _EConfigClass EConfigClass;
typedef struct _EConfigPrivate EConfigPrivate;

typedef struct _EConfigItem EConfigItem;
typedef struct _EConfigFactory EConfigFactory;
typedef struct _EConfigTarget EConfigTarget;

typedef void	(*EConfigFactoryFunc)		(EConfig *config,
						 gpointer data);

typedef gboolean
		(*EConfigCheckFunc)		(EConfig *config,
						 const gchar *pageid,
						 gpointer data);

typedef void	(*EConfigItemsFunc)		(EConfig *config,
						 GSList *items,
						 gpointer data);

typedef GtkWidget *
		(*EConfigItemFactoryFunc)	(EConfig *config,
						 EConfigItem *item,
						 GtkWidget *parent,
						 GtkWidget *old,
						 gint position,
						 gpointer data);

/* ok so this is all a bit bogussy
 * we need to map to glade stuff instead */

/* Add types?
 * if no factory, setup appropriate container ?
 * if factory, then assume that returns the right sort of object?
 * what about pages ?
*/

/**
 * enum _e_config_target_changed_t - Target changed mode.
 *
 * @E_CONFIG_TARGET_CHANGED_STATE: A state of the target has changed.
 * @E_CONFIG_TARGET_CHANGED_REBUILD: A state of the target has
 * changed, and the UI must be reconfigured as a result.
 *
 * How the target has changed.  If @E_CONFIG_TARGET_CHANGED_REBUILD then a
 * widget reconfigure is necessary, otherwise it is used to check if
 * the widget is complete yet.
 **/
typedef
enum _e_config_target_change_t {
	E_CONFIG_TARGET_CHANGED_STATE,
	E_CONFIG_TARGET_CHANGED_REBUILD
} e_config_target_change_t;

/**
 * enum _e_config_t - configuration item type.
 *
 * @E_CONFIG_BOOK: A notebook item.  Only one of this or
 * @E_CONFIG_PAGE: A configuration page.  The item @label will be
 * either the notebook tab label or the assistant page title if no factory
 * is supplied.
 * @E_CONFIG_SECTION: A section in the configuration page.  A page for
 * this section must have already been defined.  The item @label if
 * supplied will be setup as a borderless hig-compliant frame title.
 * The content of the section will be a GtkBox.  If a factory is used
 * then it is up to the factory method to create the section and add
 * it to the parent page, and return a GtkBox for following sections.
 * @E_CONFIG_SECTION_GRID: A table section.  The same as an
 * @E_CONFIG_SECTION but the content object is a GtkGrid instead.
 * @E_CONFIG_ITEM: A configuration item.  It must have a parent
 * section defined in the configuration system.
 * @E_CONFIG_ITEM_GRID: A configuration item with a parent
 * @E_CONFIG_SECTION_GRID.
 *
 * A configuration item type for each configuration item added to the
 * EConfig object.  These are merged from all contributors to the
 * configuration window, and then processed to form the combined
 * display.
 **/
enum _e_config_t {
	E_CONFIG_INVALID = -1,

	/* use one and only one of these for any given config-window id */
	E_CONFIG_BOOK,

	E_CONFIG_PAGE,
	E_CONFIG_SECTION,
	E_CONFIG_SECTION_GRID,
	E_CONFIG_ITEM,
	E_CONFIG_ITEM_GRID	/* only allowed in grid sections */
};

/**
 * struct _EConfigItem - A configuration item.
 *
 * @type: The configuration item type.
 * @path: An absolute path positioning this item in the configuration
 * window.  This will be used as a sort key for an ASCII sort to
 * position the item in the layout tree.
 * @label: A label or section title string which is used if no factory
 * is supplied to title the page or section.
 * @factory: If supplied, this will be invoked instead to create the
 * appropriate item.
 * @user_data: User data for the factory.
 *
 * The basic descriptor of a configuration item.  This may be
 * subclassed to store extra context information for each item.
 **/
struct _EConfigItem {
	enum _e_config_t type;
	gchar *path;		/* absolute path, must sort
				 * ascii-lexographically into the right spot */
	gchar *label;
	EConfigItemFactoryFunc factory;
	gpointer user_data;
};

/**
 * struct _EConfigTarget - configuration context.
 *
 * @config: The parent object.
 * @widget: A target-specific parent widget.
 * @type: The type of target, defined by implementing classes.
 *
 * The base target object is used as the parent and placeholder for
 * configuration context for a given configuration window.  It is
 * subclassed by implementing classes to provide domain-specific
 * context.
 **/
struct _EConfigTarget {
	struct _EConfig *config;
	GtkWidget *widget;	/* used if you need a parent toplevel, if available */

	guint32 type;

	/* implementation fields follow, depends on window type */
};

/**
 * struct _EConfig - A configuration management object.
 *
 * @object: Superclass.
 * @priv: Private data.
 * @id: The globally unique identifider for this configuration window,
 * used for hooking into it.
 * @target: The current target.
 * @widget: The GtkNoteBook or GtkAssistant created after
 * :create_widget() is called that represents the merged and combined
 * configuration window.
 * @window: If :create_window() is called, then the containing
 * toplevel GtkDialog or GtkWindow appropriate for the @type of
 * configuration window created.
 *
 **/
struct _EConfig {
	GObject object;
	EConfigPrivate *priv;

	gchar *id;

	EConfigTarget *target;

	GtkWidget *widget; /* the generated internal */
	GtkWidget *window; /* the window widget, GtkWindow or GtkDialog */
};

/**
 * struct _EConfigClass - Configuration management abstract class.
 *
 * @object_class: Superclass.
 * @factories: A list of factories registered on this type of
 * configuration manager.
 * @set_target: A virtual method used to set the target on the
 * configuration manager.  This is used by subclasses so they may hook
 * into changes on the target to propery drive the manager.
 * @target_free: A virtual method used to free the target in an
 * implementation-defined way.
 *
 **/
struct _EConfigClass {
	GObjectClass object_class;

	GList *factories;

	void		(*set_target)		(EConfig *config,
						 EConfigTarget *target);
	void		(*target_free)		(EConfig *config,
						 EConfigTarget *target);

	/* Signals */
	void		(*abort)		(EConfig *config);
	void		(*commit)		(EConfig *config);
};

GType		e_config_get_type		(void) G_GNUC_CONST;

/* Static class methods */
EConfigFactory *e_config_class_add_factory	(EConfigClass *klass,
						 const gchar *id,
						 EConfigFactoryFunc func,
						 gpointer user_data);

EConfig *	e_config_construct		(EConfig *config,
						 const gchar *id);

void		e_config_add_items		(EConfig *config,
						 GSList *items,
						 EConfigItemsFunc freefunc,
						 gpointer data);
void		e_config_add_page_check		(EConfig *config,
						 const gchar *pageid,
						 EConfigCheckFunc func,
						 gpointer data);

void		e_config_set_target		(EConfig *config,
						 EConfigTarget *target);
GtkWidget *	e_config_create_widget		(EConfig *config);

void		e_config_target_changed		(EConfig *config,
						 e_config_target_change_t how);

gboolean	e_config_page_check		(EConfig *config,
						 const gchar *pageid);

void		e_config_abort			(EConfig *config);
void		e_config_commit			(EConfig *config);

gpointer	e_config_target_new		(EConfig *config,
						 gint type,
						 gsize size);
void		e_config_target_free		(EConfig *config,
						 gpointer target);

G_END_DECLS

/* ********************************************************************** */

/* config plugin target, they are closely integrated */

/* To implement a basic config plugin, you just need to subclass
 * this and initialise the class target type tables */

#include <e-util/e-plugin.h>

/* Standard GObject macros */
#define E_TYPE_CONFIG_HOOK \
	(e_config_hook_get_type ())
#define E_CONFIG_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONFIG_HOOK, EConfigHook))
#define E_CONFIG_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONFIG_HOOK, EConfigHookClass))
#define E_IS_CONFIG_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONFIG_HOOK))
#define E_IS_CONFIG_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONFIG_HOOK))
#define E_CONFIG_HOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONFIG_HOOK, EConfigHookClass))

G_BEGIN_DECLS

typedef struct _EConfigHookGroup EConfigHookGroup;
typedef struct _EConfigHook EConfigHook;
typedef struct _EConfigHookClass EConfigHookClass;

typedef struct _EPluginHookTargetMap EConfigHookTargetMap;
typedef struct _EPluginHookTargetKey EConfigHookTargetMask;

typedef struct _EConfigHookItemFactoryData EConfigHookItemFactoryData;
typedef struct _EConfigHookPageCheckData EConfigHookPageCheckData;

typedef void	(*EConfigHookFunc)		(EPlugin *plugin,
						 EConfigTarget *target);
typedef void	(*EConfigHookItemFactoryFunc)	(EPlugin *plugin,
						 EConfigHookItemFactoryData *data);

/**
 * struct _EConfigHookItemFactoryData - Factory marshalling structure.
 *
 * @config: The parent EConfig.  This is also available in
 * @target->config but is here as a convenience. (TODO: do we need this).
 * @item: The corresponding configuration item.
 * @target: The current configuration target.  This is also available
 * on @config->target.
 * @parent: The parent widget for this item.  Depends on the item
 * type.
 * @old: The last widget created by this factory.  The factory is only
 * re-invoked if a reconfigure request is invoked on the EConfig.
 *
 * Used to marshal the callback data for the EConfigItemFactory method
 * to a single pointer for the EPlugin system.
 **/
struct _EConfigHookItemFactoryData {
	EConfig *config;
	EConfigItem *item;
	EConfigTarget *target;
	GtkWidget *parent;
	GtkWidget *old;
	gint position;
};

/**
 * struct _EConfigHookPageCheckData - Check callback data.
 *
 * @config:
 * @target: The current configuration target.  This is also available
 * on @config->target.
 * @pageid: Name of page to validate, or "" means check all configuration.
 *
 **/
struct _EConfigHookPageCheckData {
	EConfig *config;
	EConfigTarget *target;
	const gchar *pageid;
};

/**
 * struct _EConfigHookGroup - A group of configuration items.
 *
 * @hook: Parent object.
 * @id: The configuration window to which these items apply.
 * @target_type: The target type expected by the items.  This is
 * defined by implementing classes.
 * @items: A list of EConfigHookItem's for this group.
 * @check: A validate page handler.
 * @commit: The name of the commit function for this group of items, or NULL
 * for instant-apply configuration windows.  Its format is plugin-type defined.
 * @abort: Similar to the @commit function but for aborting or
 * cancelling a configuration edit.
 *
 * Each plugin that hooks into a given configuration page will define
 * all of the items for that page in a single group.
 **/
struct _EConfigHookGroup {
	struct _EConfigHook *hook; /* parent pointer */
	gchar *id;		/* target menu id for these config items */
	gint target_type;	/* target type of this group */
	GSList *items;		/* items to add to group */
	gchar *check;		/* validate handler, if set */
	gchar *commit;		/* commit handler, if set */
	gchar *abort;		/* abort handler, if set */
};

/**
 * struct _EConfigHook - Plugin hook for configuration windows.
 *
 * @hook: Superclass.
 * @groups: A list of EConfigHookGroup's of all configuration windows
 * this plugin hooks into.
 *
 **/
struct _EConfigHook {
	EPluginHook hook;

	GSList *groups;
};

/**
 * EConfigHookClass:
 * @hook_class: Superclass.
 * @target_map: A table of EConfigHookTargetMap structures describing
 * the possible target types supported by this class.
 * @config_class: The EConfig derived class that this hook
 * implementation drives.
 *
 * This is an abstract class defining the plugin hook point for
 * configuration windows.
 *
 **/
struct _EConfigHookClass {
	EPluginHookClass hook_class;

	/* EConfigHookTargetMap by .type */
	GHashTable *target_map;
	/* the config class these configs's belong to */
	EConfigClass *config_class;
};

GType e_config_hook_get_type (void);

/* for implementors */
void		e_config_hook_class_add_target_map
					(EConfigHookClass *hook_class,
					 const EConfigHookTargetMap *map);

G_END_DECLS

#endif /* E_CONFIG_H */
