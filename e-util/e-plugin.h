
#ifndef _E_PLUGIN_H
#define _E_PLUGIN_H

#include <glib.h>
#include <glib-object.h>
#include <libxml/tree.h>

/* ********************************************************************** */

typedef struct _EPlugin EPlugin;
typedef struct _EPluginClass EPluginClass;

#define E_PLUGIN_CLASSID "com.ximian.evolution.plugin"

/**
 * struct _EPlugin - An EPlugin instance.
 * 
 * @object: Superclass.
 * @id: Unique identifier for plugin instance.
 * @path: Filename where the xml definition resides.
 * @hooks_pending: A list hooks which can't yet be loaded.  This is
 * the xmlNodePtr to the root node of the hook definition.
 * @description: A description of the plugin's purpose.
 * @name: The name of the plugin.
 * @domain: The translation domain for this plugin.
 * @hooks: A list of the EPluginHooks this plugin requires.
 * @enabled: Whether the plugin is enabled or not.  This is not fully
 * implemented.
 * 
 * The base EPlugin object is used to represent each plugin directly.
 * All of the plugin's hooks are loaded and managed through this
 * object.
 **/
struct _EPlugin {
	GObject object;

	char *id;
	char *path;
	GSList *hooks_pending;

	char *description;
	char *name;
	char *domain;
	GSList *hooks;

	int enabled:1;
};

/**
 * struct _EPluginClass - 
 * 
 * @class: Superclass.
 * @type: The plugin type.  This is used by the plugin loader to
 * determine which plugin object to instantiate to handle the plugin.
 * This must be overriden by each subclass to provide a unique name.
 * @construct: The construct virtual method scans the XML tree to
 * initialise itself.
 * @invoke: The invoke virtual method loads the plugin code, resolves
 * the function name, and marshals a simple pointer to execute the
 * plugin.
 *
 * The EPluginClass represents each plugin type.  The type of each class is
 * registered in a global table and is used to instantiate a
 * container for each plugin.
 *
 * It provides two main functions, to load the plugin definition, and
 * to invoke a function.  Each plugin class is used to handle mappings
 * to different languages.
 **/
struct _EPluginClass {
	GObjectClass class;

	const char *type;

	int (*construct)(EPlugin *, xmlNodePtr root);
	void *(*invoke)(EPlugin *, const char *name, void *data);
};

GType e_plugin_get_type(void);

int e_plugin_construct(EPlugin *ep, xmlNodePtr root);
void e_plugin_add_load_path(const char *);
int e_plugin_load_plugins(void);

void e_plugin_register_type(GType type);

void *e_plugin_invoke(EPlugin *ep, const char *name, void *data);
void e_plugin_enable(EPlugin *eph, int state);

/* static helpers */
/* maps prop or content to 'g memory' */
char *e_plugin_xml_prop(xmlNodePtr node, const char *id);
char *e_plugin_xml_prop_domain(xmlNodePtr node, const char *id, const char *domain);
int e_plugin_xml_int(xmlNodePtr node, const char *id, int def);
char *e_plugin_xml_content(xmlNodePtr node);
char *e_plugin_xml_content_domain(xmlNodePtr node, const char *domain);

/* ********************************************************************** */
#include <gmodule.h>

typedef struct _EPluginLib EPluginLib;
typedef struct _EPluginLibClass EPluginLibClass;

/* The callback signature used for epluginlib methods */
typedef void *(*EPluginLibFunc)(EPluginLib *ep, void *data);
/* The setup method, this will be called when the plugin is
 * initialised.  In the future it may also be called when the plugin
 * is disabled. */
typedef int (*EPluginLibEnableFunc)(EPluginLib *ep, int enable);

/**
 * struct _EPluginLib - 
 * 
 * @plugin: Superclass.
 * @location: The filename of the shared object.
 * @module: The GModule once it is loaded.
 * 
 * This is a concrete EPlugin class.  It loads and invokes dynamically
 * loaded libraries using GModule.  The shared object isn't loaded
 * until the first callback is invoked.
 *
 * When the plugin is loaded, and if it exists, "e_plugin_lib_enable"
 * will be invoked to initialise the 
 **/
struct _EPluginLib {
	EPlugin plugin;

	char *location;
	GModule *module;
};

/**
 * struct _EPluginLibClass - 
 * 
 * @plugin_class: Superclass.
 * 
 * The plugin library needs no additional class data.
 **/
struct _EPluginLibClass {
	EPluginClass plugin_class;
};

GType e_plugin_lib_get_type(void);

/* ********************************************************************** */

typedef struct _EPluginHook EPluginHook;
typedef struct _EPluginHookClass EPluginHookClass;

/* utilities for subclasses to use */
typedef struct _EPluginHookTargetMap EPluginHookTargetMap;
typedef struct _EPluginHookTargetKey EPluginHookTargetKey;

/**
 * struct _EPluginHookTargetKey - 
 * 
 * @key: Enumeration value as a string.
 * @value: Enumeration value as an integer.
 * 
 * A multi-purpose string to id mapping structure used with various
 * helper functions to simplify plugin hook subclassing.
 **/
struct _EPluginHookTargetKey {
	const char *key;
	guint32 value;
};

/**
 * struct _EPluginHookTargetMap - 
 * 
 * @type: The string id of the target.
 * @id: The integer id of the target.  Maps directly to the type field
 * of the various plugin type target id's.
 * @mask_bits: A zero-fill terminated array of EPluginHookTargetKeys.
 *
 * Used by EPluginHook to define mappings of target type enumerations
 * to and from strings.  Also used to define the mask option names
 * when reading the XML plugin hook definitions.
 **/
struct _EPluginHookTargetMap {
	const char *type;
	int id;
	const struct _EPluginHookTargetKey *mask_bits;	/* null terminated array */
};

/**
 * struct _EPluginHook - A plugin hook.
 * 
 * @object: Superclass.
 * @plugin: The parent object.
 * 
 * An EPluginHook is used as a container for each hook a given plugin
 * is listening to.
 **/
struct _EPluginHook {
	GObject object;

	struct _EPlugin *plugin;
};

/**
 * struct _EPluginHookClass - 
 * 
 * @class: Superclass.
 * @id: The plugin hook type. This must be overriden by each subclass
 * and is used as a key when loading hook definitions.  This string
 * should contain a globally unique name followed by a : and a version
 * specification.  This is to ensure plugins only hook into hooks with
 * the right API.
 * @construct: Virtual method used to initialise the object when
 * loaded.
 * @enable: Virtual method used to enable or disable the hook.
 * 
 * The EPluginHookClass represents each hook type.  The type of the
 * class is registered in a global table and is used to instantiate a
 * container for each hook.
 **/
struct _EPluginHookClass {
	GObjectClass class;

	const char *id;

	int (*construct)(EPluginHook *eph, EPlugin *ep, xmlNodePtr root);
	void (*enable)(EPluginHook *eph, int state);
};

GType e_plugin_hook_get_type(void);

void e_plugin_hook_register_type(GType type);

EPluginHook * e_plugin_hook_new(EPlugin *ep, xmlNodePtr root);
void e_plugin_hook_enable(EPluginHook *eph, int state);

/* static methods */
guint32 e_plugin_hook_mask(xmlNodePtr root, const struct _EPluginHookTargetKey *map, const char *prop);
guint32 e_plugin_hook_id(xmlNodePtr root, const struct _EPluginHookTargetKey *map, const char *prop);

#endif /* ! _E_PLUGIN_H */
