
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include <glib/gi18n.h>

#include "e-plugin.h"
#include "e-msgport.h"

/* plugin debug */
#define pd(x) x
/* plugin hook debug */
#define phd(x) x

/*
<camel-plugin
  class="org.gnome.camel.plugin.provider:1.0"
  id="org.gnome.camel.provider.imap:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  factory="camel_imap_provider_new">
 <name>imap</name>
 <description>IMAP4 and IMAP4v1 mail store</description>
 <class-data class="org.gnome.camel.plugin.provider:1.0"
   protocol="imap"
   domain="mail"
   flags="remote,source,storage,ssl"/>
</camel-plugin>

<camel-plugin
  class="org.gnome.camel.plugin.sasl:1.0"
  id="org.gnome.camel.sasl.plain:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelsasl.so"
  factory="camel_sasl_plain_new">
 <name>PLAIN</name>
 <description>SASL PLAIN authentication mechanism</description>
</camel-plugin>
*/

/* EPlugin stuff */
static GObjectClass *ep_parent_class;

/* global table of plugin types by pluginclass.type */
static GHashTable *ep_types;
/* plugin load path */
static GSList *ep_path;
/* global table of plugins by plugin.id */
static GHashTable *ep_plugins;
/* a table of GSLists of plugins by hook class for hooks not loadable yet */
static GHashTable *ep_plugins_pending;
/* list of all cached xml docs:struct _plugin_doc's */
static EDList ep_plugin_docs = E_DLIST_INITIALISER(ep_plugin_docs);

/* EPluginHook stuff */
static void *eph_parent_class;
/* All classes which implement EPluginHooks, by class.id */
static GHashTable *eph_types;

struct _plugin_doc {
	struct _plugin_doc *next;
	struct _plugin_doc *prev;

	xmlDocPtr doc;
	GSList *plugins;
};

static int
ep_construct(EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;
	int res = -1;

	ep->domain = e_plugin_xml_prop(root, "domain");
	ep->name = e_plugin_xml_prop_domain(root, "name", ep->domain);

	printf("creating plugin '%s' '%s'\n", ep->name?ep->name:"un-named", ep->id);

	node = root->children;
	while (node) {
		if (strcmp(node->name, "hook") == 0) {
			struct _EPluginHook *hook;
			EPluginHookClass *type;
			char *class = e_plugin_xml_prop(node, "class");

			if (class == NULL) {
				g_warning("Plugin '%s' load failed in '%s', missing class property for hook", ep->id, ep->path);
				goto fail;
			}

			if (eph_types != NULL
			    && (type = g_hash_table_lookup(eph_types, class)) != NULL) {
				g_free(class);
				hook = g_object_new(G_OBJECT_CLASS_TYPE(type), NULL);
				res = type->construct(hook, ep, node);
				if (res == -1) {
					g_warning("Plugin '%s' failed to load hook", ep->name);
					g_object_unref(hook);
					goto fail;
				} else {
					ep->hooks = g_slist_append(ep->hooks, hook);
				}
			} else {
				GSList *l;
				char *oldclass;

				if (ep_plugins_pending == NULL)
					ep_plugins_pending = g_hash_table_new(g_str_hash, g_str_equal);
				if (!g_hash_table_lookup_extended(ep_plugins_pending, class, (void **)&oldclass, (void **)&l)) {
					oldclass = class;
					l = NULL;
				}
				l = g_slist_prepend(l, ep);
				g_hash_table_insert(ep_plugins_pending, oldclass, l);
				ep->hooks_pending = g_slist_prepend(ep->hooks_pending, node);
			}
		} else if (strcmp(node->name, "description") == 0) {
			ep->description = e_plugin_xml_content_domain(node, ep->domain);
		}
		node = node->next;
	}
	res = 0;
fail:
	return res;
}

static void
ep_finalise(GObject *o)
{
	EPlugin *ep = (EPlugin *)o;

	g_free(ep->id);
	g_free(ep->description);
	g_free(ep->name);
	g_free(ep->domain);
	g_slist_free(ep->hooks_pending);

	g_slist_foreach(ep->hooks, (GFunc)g_object_unref, NULL);
	g_slist_free(ep->hooks);

	((GObjectClass *)ep_parent_class)->finalize(o);
}

static void
ep_init(GObject *o)
{
	EPlugin *ep = (EPlugin *)o;

	ep->enabled = TRUE;
}

static void
ep_class_init(EPluginClass *klass)
{
	((GObjectClass *)klass)->finalize = ep_finalise;
	klass->construct = ep_construct;
}

/**
 * e_plugin_get_type:
 * 
 * Standard GObject type function.  This is only an abstract class, so
 * you can only use this to subclass EPlugin.
 * 
 * Return value: The type.
 **/
GType
e_plugin_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		char *path, *col, *p;

		static const GTypeInfo info = {
			sizeof(EPluginClass), NULL, NULL, (GClassInitFunc)ep_class_init, NULL, NULL,
			sizeof(EPlugin), 0, (GInstanceInitFunc)ep_init,
		};

		ep_parent_class = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EPlugin", &info, 0);

		/* Add paths in the environment variable or default global and user specific paths */
		path = g_strdup(getenv("EVOLUTION_PLUGIN_PATH"));
		if (path == NULL) {
			/* Add the global path */
			e_plugin_add_load_path(EVOLUTION_PLUGINDIR);

			path = g_build_filename(g_get_home_dir(), ".eplugins", NULL);
		}
		
		p = path;
		while ((col = strchr(p, ':'))) {
			*col++ = 0;
			e_plugin_add_load_path(p);
			p = col;
		}
		e_plugin_add_load_path(p);
		g_free(path);
	}
	
	return type;
}

static int
ep_load(const char *filename)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	int res = -1;
	EPlugin *ep;
	int cache = FALSE;
	struct _plugin_doc *pdoc;

	doc = xmlParseFile(filename);
	if (doc == NULL) {
		return -1;
	}

	root = xmlDocGetRootElement(doc);
	if (strcmp(root->name, "e-plugin-list") != 0) {
		g_warning("No <e-plugin-list> root element: %s", filename);
		xmlFreeDoc(doc);
		return -1;
	}

	pdoc = g_malloc0(sizeof(*pdoc));
	pdoc->doc = doc;
	pdoc->plugins = NULL;

	for (root = root->children; root ; root = root->next) {
		if (strcmp(root->name, "e-plugin") == 0) {
			char *prop, *id;
			EPluginClass *klass;

			id = e_plugin_xml_prop(root, "id");
			if (id == NULL) {
				g_warning("Invalid e-plugin entry in '%s': no id", filename);
				goto fail;
			}

			if (g_hash_table_lookup(ep_plugins, id)) {
				g_warning("Plugin '%s' already defined", id);
				g_free(id);
				continue;
			}

			prop = xmlGetProp(root, "type");
			if (prop == NULL) {
				g_free(id);
				g_warning("Invalid e-plugin entry in '%s': no type", filename);
				goto fail;
			}

			klass = g_hash_table_lookup(ep_types, prop);
			if (klass == NULL) {
				g_warning("Can't find plugin type '%s' for plugin '%s'\n", prop, id);
				g_free(id);
				xmlFree(prop);
				continue;
			}
			xmlFree(prop);

			ep = g_object_new(G_TYPE_FROM_CLASS(klass), NULL);
			ep->id = id;
			ep->path = g_strdup(filename);
			if (e_plugin_construct(ep, root) == -1) {
				g_object_unref(ep);
			} else {
				g_hash_table_insert(ep_plugins, ep->id, ep);
				pdoc->plugins = g_slist_prepend(pdoc->plugins, ep);
				cache |= (ep->hooks_pending != NULL);
			}
		}
	}

	res = 0;
fail:
	if (cache) {
		printf("Caching plugin description '%s' for unknown future hooks\n", filename);
		e_dlist_addtail(&ep_plugin_docs, (EDListNode *)pdoc);
	} else {
		xmlFreeDoc(pdoc->doc);
		g_free(pdoc);
	}

	return res;
}

/* This loads a hook that was pending on a given plugin but the type wasn't registered yet */
/* This works in conjunction with ep_construct and e_plugin_hook_register_type to make sure
   everything works nicely together.  Apparently. */
static int
ep_load_pending(EPlugin *ep, EPluginHookClass *type)
{
	int res = 0;
	GSList *l, *p;

	printf("New hook type registered '%s', loading pending hooks on plugin '%s'\n", type->id, ep->id);

	l = ep->hooks_pending;
	p = NULL;
	while (l) {
		GSList *n = l->next;
		xmlNodePtr node = l->data;
		char *class = xmlGetProp(node, "class");
		EPluginHook *hook;

		printf(" checking pending hook '%s'\n", class?class:"<unknown>");

		if (class) {
			if (strcmp(class, type->id) == 0) {
				hook = g_object_new(G_OBJECT_CLASS_TYPE(type), NULL);
				res = type->construct(hook, ep, node);
				if (res == -1) {
					g_warning("Plugin '%s' failed to load hook '%s'", ep->name, type->id);
					g_object_unref(hook);
				} else {
					ep->hooks = g_slist_append(ep->hooks, hook);
				}

				if (p)
					p->next = n;
				else
					ep->hooks_pending = n;
				g_slist_free_1(l);
				l = p;
			}

			xmlFree(class);
		}

		p = l;
		l = n;
	}

	return res;
}

/**
 * e_plugin_add_load_path:
 * @path: The path to add to search for plugins.
 * 
 * Add a path to be searched when e_plugin_load_plugins() is called.
 * By default the system plugin directory and ~/.eplugins is used as
 * the search path unless overriden by the environmental variable
 * %EVOLUTION_PLUGIN_PATH.
 *
 * %EVOLUTION_PLUGIN_PATH is a : separated list of paths to search for
 * plugin definitions in order.
 *
 * Plugin definitions are XML files ending in the extension ".eplug".
 **/
void
e_plugin_add_load_path(const char *path)
{
	ep_path = g_slist_append(ep_path, g_strdup(path));
}

/**
 * e_plugin_load_plugins:
 * 
 * Scan the search path, looking for plugin definitions, and load them
 * into memory.
 * 
 * Return value: Returns -1 if an error occured.
 **/
int
e_plugin_load_plugins(void)
{
	GSList *l;

	if (ep_types == NULL) {
		g_warning("no plugin types defined");
		return 0;
	}


	for (l = ep_path;l;l = g_slist_next(l)) {
		DIR *dir;
		struct dirent *d;
		char *path = l->data;

		printf("scanning plugin dir '%s'\n", path);

		dir = opendir(path);
		if (dir == NULL) {
			g_warning("Could not find plugin path: %s", path);
			continue;
		}

		while ( (d = readdir(dir)) ) {
			if (strlen(d->d_name) > 6
			    && !strcmp(d->d_name + strlen(d->d_name) - 6, ".eplug")) {
				char * name = g_build_filename(path, d->d_name, NULL);

				ep_load(name);
				g_free(name);
			}
		}

		closedir(dir);
	}

	return 0;
}

/**
 * e_plugin_register_type:
 * @type: The GObject type of the plugin loader.
 * 
 * Register a new plugin type with the plugin system.  Each type must
 * subclass EPlugin and must override the type member of the
 * EPluginClass with a unique name.
 **/
void
e_plugin_register_type(GType type)
{
	EPluginClass *klass;

	if (ep_types == NULL) {
		ep_types = g_hash_table_new(g_str_hash, g_str_equal);
		ep_plugins = g_hash_table_new(g_str_hash, g_str_equal);
	}

	klass = g_type_class_ref(type);

	pd(printf("register plugin type '%s'\n", klass->type));

	g_hash_table_insert(ep_types, (void *)klass->type, klass);
}

/**
 * e_plugin_construct:
 * @ep: An EPlugin derived object.
 * @root: The XML root node of the sub-tree containing the plugin
 * definition.
 * 
 * Helper to invoke the construct virtual method.
 * 
 * Return value: The return from the construct virtual method.
 **/
int
e_plugin_construct(EPlugin *ep, xmlNodePtr root)
{
	return ((EPluginClass *)G_OBJECT_GET_CLASS(ep))->construct(ep, root);
}

/**
 * e_plugin_invoke:
 * @ep: 
 * @name: The name of the function to invoke. The format of this name
 * will depend on the EPlugin type and its language conventions.
 * @data: The argument to the function. Its actual type depends on
 * the hook on which the function resides. It is up to the called
 * function to get this right.
 * 
 * Helper to invoke the invoke virtual method.
 * 
 * Return value: The return of the plugin invocation.
 **/
void *
e_plugin_invoke(EPlugin *ep, const char *name, void *data)
{
	if (!ep->enabled)
		g_warning("Invoking method on disabled plugin");

	return ((EPluginClass *)G_OBJECT_GET_CLASS(ep))->invoke(ep, name, data);
}

/**
 * e_plugin_enable:
 * @ep: 
 * @state: 
 * 
 * Set the enable state of a plugin.
 *
 * THIS IS NOT FULLY IMPLEMENTED YET
 **/
void
e_plugin_enable(EPlugin *ep, int state)
{
	GSList *l;

	if ((ep->enabled == 0) == (state == 0))
		return;

	ep->enabled = state;
	for (l=ep->hooks;l;l = g_slist_next(l)) {
		EPluginHook *eph = l->data;

		e_plugin_hook_enable(eph, state);
	}
}

/**
 * e_plugin_xml_prop:
 * @node: An XML node.
 * @id: The name of the property to retrieve.
 * 
 * A static helper function to look up a property on an XML node, and
 * ensure it is allocated in GLib system memory.  If GLib isn't using
 * the system malloc then it must copy the property value.
 * 
 * Return value: The property, allocated in GLib memory, or NULL if no
 * such property exists.
 **/
char *
e_plugin_xml_prop(xmlNodePtr node, const char *id)
{
	char *p = xmlGetProp(node, id);

	if (g_mem_is_system_malloc()) {
		return p;
	} else {
		char * out = g_strdup(p);

		if (p)
			xmlFree(p);
		return out;
	}
}

/**
 * e_plugin_xml_prop_domain:
 * @node: An XML node.
 * @id: The name of the property to retrieve.
 * @domain: The translation domain for this string.
 * 
 * A static helper function to look up a property on an XML node, and
 * translate it based on @domain.
 * 
 * Return value: The property, allocated in GLib memory, or NULL if no
 * such property exists.
 **/
char *
e_plugin_xml_prop_domain(xmlNodePtr node, const char *id, const char *domain)
{
	char *p, *out;

	p = xmlGetProp(node, id);
	if (p == NULL)
		return NULL;

	out = g_strdup(dgettext(domain, p));
	xmlFree(p);

	return out;
}

/**
 * e_plugin_xml_int:
 * @node: An XML node.
 * @id: The name of the property to retrieve.
 * @def: A default value if the property doesn't exist.  Can be used
 * to determine if the property isn't set.
 * 
 * A static helper function to look up a property on an XML node as an
 * integer.  If the property doesn't exist, then @def is returned as a
 * default value instead.
 * 
 * Return value: The value if set, or @def if not.
 **/
int
e_plugin_xml_int(xmlNodePtr node, const char *id, int def)
{
	char *p = xmlGetProp(node, id);

	if (p)
		return atoi(p);
	else
		return def;
}

/**
 * e_plugin_xml_content:
 * @node: 
 * 
 * A static helper function to retrieve the entire textual content of
 * an XML node, and ensure it is allocated in GLib system memory.  If
 * GLib isn't using the system malloc them it must copy the content.
 * 
 * Return value: The node content, allocated in GLib memory.
 **/
char *
e_plugin_xml_content(xmlNodePtr node)
{
	char *p = xmlNodeGetContent(node);

	if (g_mem_is_system_malloc()) {
		return p;
	} else {
		char * out = g_strdup(p);

		if (p)
			xmlFree(p);
		return out;
	}
}

/**
 * e_plugin_xml_content_domain:
 * @node: 
 * @domain:
 * 
 * A static helper function to retrieve the entire textual content of
 * an XML node, and ensure it is allocated in GLib system memory.  If
 * GLib isn't using the system malloc them it must copy the content.
 * 
 * Return value: The node content, allocated in GLib memory.
 **/
char *
e_plugin_xml_content_domain(xmlNodePtr node, const char *domain)
{
	char *p, *out;

	p = xmlNodeGetContent(node);
	if (p == NULL)
		return NULL;

	out = g_strdup(dgettext(domain, p));
	xmlFree(p);

	return out;
}

/* ********************************************************************** */
static void *epl_parent_class;

/* this looks weird, but it saves a lot of typing */
#define epl ((EPluginLib *)ep)

/* TODO:
   We need some way to manage lifecycle.
   We need some way to manage state.

   Maybe just the g module init method will do, or we could add
   another which returns context.

   There is also the question of per-instance context, e.g. for config
   pages.
*/

static void *
epl_invoke(EPlugin *ep, const char *name, void *data)
{
	EPluginLibFunc cb;

	if (!ep->enabled) {
		g_warning("trying to invoke '%s' on disabled plugin '%s'", name, ep->id);
		return NULL;
	}

	if (epl->module == NULL) {
		EPluginLibEnableFunc enable;
		
		if ((epl->module = g_module_open(epl->location, 0)) == NULL) {
			g_warning("can't load plugin '%s'", g_module_error());
			return NULL;
		}

		if (g_module_symbol(epl->module, "e_plugin_lib_enable", (void *)&enable)) {
			if (enable(epl, TRUE) != 0) {
				ep->enabled = FALSE;
				g_module_close(epl->module);
				epl->module = NULL;
				return NULL;
			}
		}
	}

	if (!g_module_symbol(epl->module, name, (void *)&cb)) {
		g_warning("Cannot resolve symbol '%s' in plugin '%s' (not exported?)", name, epl->location);
		return NULL;
	}

	return cb(epl, data);
}

static int
epl_construct(EPlugin *ep, xmlNodePtr root)
{
	if (((EPluginClass *)epl_parent_class)->construct(ep, root) == -1)
		return -1;

	epl->location = e_plugin_xml_prop(root, "location");

	if (epl->location == NULL)
		return -1;

	return 0;
}

static void
epl_finalise(GObject *o)
{
	EPlugin *ep = (EPlugin *)o;

	g_free(epl->location);

	if (epl->module)
		g_module_close(epl->module);

	((GObjectClass *)epl_parent_class)->finalize(o);
}

static void
epl_class_init(EPluginClass *klass)
{
	((GObjectClass *)klass)->finalize = epl_finalise;
	klass->construct = epl_construct;
	klass->invoke = epl_invoke;
	klass->type = "shlib";
}

/**
 * e_plugin_lib_get_type:
 * 
 * Standard GObject function to retrieve the EPluginLib type.  Use to
 * register the type with the plugin system if you want to use shared
 * library plugins.
 * 
 * Return value: The EPluginLib type.
 **/
GType
e_plugin_lib_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EPluginLibClass), NULL, NULL, (GClassInitFunc) epl_class_init, NULL, NULL,
			sizeof(EPluginLib), 0, (GInstanceInitFunc) NULL,
		};

		epl_parent_class = g_type_class_ref(e_plugin_get_type());
		type = g_type_register_static(e_plugin_get_type(), "EPluginLib", &info, 0);
	}
	
	return type;
}

/* ********************************************************************** */

static int
eph_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	eph->plugin = ep;

	return 0;
}

static void
eph_enable(EPluginHook *eph, int state)
{
	/* NOOP */
}

static void
eph_finalise(GObject *o)
{
	((GObjectClass *)eph_parent_class)->finalize((GObject *)o);
}

static void
eph_class_init(EPluginHookClass *klass)
{
	((GObjectClass *)klass)->finalize = eph_finalise;
	klass->construct = eph_construct;
	klass->enable = eph_enable;
}

/**
 * e_plugin_hook_get_type:
 * 
 * Standard GObject function to retrieve the EPluginHook type.  Since
 * EPluginHook is an abstract class, this is only used to subclass it.
 * 
 * Return value: The EPluginHook type.
 **/
GType
e_plugin_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EPluginHookClass), NULL, NULL, (GClassInitFunc) eph_class_init, NULL, NULL,
			sizeof(EPluginHook), 0, (GInstanceInitFunc) NULL,
		};

		eph_parent_class = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EPluginHook", &info, 0);
	}
	
	return type;
}

/**
 * e_plugin_hook_enable: Set hook enabled state.
 * @eph: 
 * @state: 
 * 
 * Set the enabled state of the plugin hook.  This is called by the
 * plugin code.
 *
 * THIS IS NOT FULY IMEPLEMENTED YET
 **/
void
e_plugin_hook_enable(EPluginHook *eph, int state)
{
	((EPluginHookClass *)G_OBJECT_GET_CLASS(eph))->enable(eph, state);
}

/**
 * e_plugin_hook_register_type:
 * @type: 
 * 
 * Register a new plugin hook type with the plugin system.  Each type
 * must subclass EPluginHook and must override the id member of the
 * EPluginHookClass with a unique identification string.
 **/
void
e_plugin_hook_register_type(GType type)
{
	EPluginHookClass *klass, *oldklass;
	GSList *l, *plugins;
	char *class;

	if (eph_types == NULL)
		eph_types = g_hash_table_new(g_str_hash, g_str_equal);

	klass = g_type_class_ref(type);

	oldklass = g_hash_table_lookup(eph_types, (void *)klass->id);
	if (oldklass == klass) {
		g_type_class_unref(klass);
		return;
	} else if (oldklass != NULL) {
		g_warning("Trying to re-register hook type '%s'", klass->id);
		return;
	}

	phd(printf("register plugin hook type '%s'\n", klass->id));
	g_hash_table_insert(eph_types, (void *)klass->id, klass);

	/* if we've already loaded a plugin that needed this hook but it didn't exist, re-load it now */

	if (ep_plugins_pending
	    && g_hash_table_lookup_extended(ep_plugins_pending, klass->id, (void **)&class, (void **)&plugins)) {
		struct _plugin_doc *pdoc, *ndoc;

		g_hash_table_remove(ep_plugins_pending, class);
		g_free(class);
		for (l = plugins; l; l = g_slist_next(l)) {
			EPlugin *ep = l->data;

			ep_load_pending(ep, klass);
		}
		g_slist_free(plugins);

		/* See if we can now garbage collect the xml definition since its been fully loaded */

		/* This is all because libxml doesn't refcount! */

		pdoc = (struct _plugin_doc *)ep_plugin_docs.head;
		ndoc = pdoc->next;
		while (ndoc) {
			if (pdoc->doc) {
				int cache = FALSE;

				for (l=pdoc->plugins;l;l=g_slist_next(l))
					cache |= (((EPlugin *)l->data)->hooks_pending != NULL);

				if (!cache) {
					printf("Gargabe collecting plugin description\n");
					e_dlist_remove((EDListNode *)pdoc);
					xmlFreeDoc(pdoc->doc);
					g_free(pdoc);
				}
			}

			pdoc = ndoc;
			ndoc = ndoc->next;
		}
	}
}

/**
 * e_plugin_hook_mask:
 * @root: An XML node.
 * @map: A zero-fill terminated array of EPluginHookTargeKeys used to
 * map a string with a bit value.
 * @prop: The property name.
 * 
 * This is a static helper function which looks up a property @prop on
 * the XML node @root, and then uses the @map table to convert it into
 * a bitmask.  The property value is a comma separated list of
 * enumeration strings which are indexed into the @map table.
 *
 * Return value: A bitmask representing the inclusive-or of all of the
 * integer values of the corresponding string id's stored in the @map.
 **/
guint32
e_plugin_hook_mask(xmlNodePtr root, const struct _EPluginHookTargetKey *map, const char *prop)
{
	char *val, *p, *start, c;
	guint32 mask = 0;

	val = xmlGetProp(root, prop);
	if (val == NULL)
		return 0;

	p = val;
	do {
		start = p;
		while (*p && *p != ',')
			p++;
		c = *p;
		*p = 0;
		if (start != p) {
			int i;

			for (i=0;map[i].key;i++) {
				if (!strcmp(map[i].key, start)) {
					mask |= map[i].value;
					break;
				}
			}
		}
		*p++ = c;
	} while (c);

	xmlFree(val);

	return mask;
}

/**
 * e_plugin_hook_id:
 * @root: 
 * @map: 
 * @prop: 
 * 
 * This is a static helper function which looks up a property @prop on
 * the XML node @root, and then uses the @map table to convert it into
 * an integer.
 *
 * This is used as a helper wherever you need to represent an
 * enumerated value in the XML.
 * 
 * Return value: If the @prop value is in @map, then the corresponding
 * integer value, if not, then ~0.
 **/
guint32
e_plugin_hook_id(xmlNodePtr root, const struct _EPluginHookTargetKey *map, const char *prop)
{
	char *val;
	int i;

	val = xmlGetProp(root, prop);
	if (val == NULL)
		return ~0;

	for (i=0;map[i].key;i++) {
		if (!strcmp(map[i].key, val)) {
			xmlFree(val);
			return map[i].value;
		}
	}

	xmlFree(val);

	return ~0;
}

#if 0
/*
  e-mail-format-handler
    mime_type
    target
*/
struct _EMFormatPlugin {
	EPlugin plugin;

	char *target;
	char *mime_type;
	struct _EMFormatHandler *(*get_handler)(void);
};

struct _EMFormatPluginClass {
	EPluginClass plugin_class;
};

#endif

#if 0
void em_setup_plugins(void);

void
em_setup_plugins(void)
{
	GType *e_plugin_mono_get_type(void);

	e_plugin_register_type(e_plugin_lib_get_type());
	e_plugin_register_type(e_plugin_mono_get_type());

	e_plugin_hook_register_type(em_popup_hook_get_type());

	e_plugin_load_plugins(".");
}
#endif
