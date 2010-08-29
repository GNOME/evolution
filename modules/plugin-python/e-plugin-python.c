/*
 * e-plugin-python.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* Include <Python.h> first to avoid:
 * warning: "_POSIX_C_SOURCE" redefined */
#include <Python.h>

#include "e-plugin-python.h"

#include <sys/types.h>
#include <string.h>

#define E_PLUGIN_PYTHON_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_PLUGIN_PYTHON, EPluginPythonPrivate))

struct _EPluginPythonPrivate {
	PyObject *pModule;
	PyObject *pClass;
	PyObject *pFunc;
	PyObject *pDict;
	GHashTable *methods;
};

static gpointer parent_class;
static GType plugin_python_type;

static gchar *
get_xml_prop (xmlNodePtr node, const gchar *id)
{
	xmlChar *prop;
	gchar *out = NULL;

	prop = xmlGetProp (node, (xmlChar *) id);

	if (prop != NULL) {
		out = g_strdup ((gchar *) prop);
		xmlFree (prop);
	}

	return out;
}

static void
plugin_python_finalize (GObject *object)
{
	EPluginPython *plugin_python;

	plugin_python = E_PLUGIN_PYTHON (object);

	g_free (plugin_python->location);
	g_free (plugin_python->module_name);
	g_free (plugin_python->pClass);

	g_hash_table_destroy (plugin_python->priv->methods);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
plugin_python_construct (EPlugin *plugin, xmlNodePtr root)
{
	EPluginPython *plugin_python;

	/* Chain up to parent's construct() method. */
	if (E_PLUGIN_CLASS (parent_class)->construct (plugin, root) == -1)
		return -1;

	plugin_python = E_PLUGIN_PYTHON (plugin);
	plugin_python->location = get_xml_prop (root, "location");
	plugin_python->module_name = get_xml_prop (root, "module_name");
	plugin_python->pClass = get_xml_prop (root, "pClass");

	return (plugin_python->location != NULL) ? 0 : -1;
}

static gpointer
plugin_python_invoke (EPlugin *plugin,
                      const gchar *name,
                      gpointer data)
{
	EPluginPython *plugin_python;
	EPluginPythonPrivate *priv;
	PyObject *pModuleName, *pFunc;
	PyObject *pValue = NULL;

	plugin_python = E_PLUGIN_PYTHON (plugin);
	priv = plugin_python->priv;

	/* We need to do this every time since we may be called
	 * from any thread for some uses. */
	Py_Initialize ();

	if (priv->pModule == NULL) {
		gchar *string;

		pModuleName = PyString_FromString (plugin_python->module_name);

		string = g_strdup_printf (
			"import sys; "
			"sys.path.insert(0, '%s')",
			plugin_python->location);
		PyRun_SimpleString (string);
		g_free (string);

		priv->pModule = PyImport_Import (pModuleName);

		Py_DECREF (pModuleName); //Free

		if (priv->pModule == NULL) {
			PyErr_Print ();
			g_warning (
				"Can't load python module '%s'",
				plugin_python->location);
			return NULL;
		}

		priv->pDict = PyModule_GetDict (priv->pModule);

		if (plugin_python->pClass != NULL) {
			priv->pClass = PyDict_GetItemString (
				priv->pDict, plugin_python->pClass);
		}
	}

	if (priv->pClass) {

		if (PyCallable_Check (priv->pClass)) {
			PyObject *pInstance;

			pInstance = PyObject_CallObject (priv->pClass, NULL);
			pValue = PyObject_CallMethod (pInstance, (gchar *) name, NULL);
		}

	} else {

		pFunc = PyDict_GetItemString (priv->pDict, name);

		if (pFunc && PyCallable_Check (pFunc))
			pValue = PyObject_CallObject (pFunc, NULL);
		else
			PyErr_Print ();
	}

	if (pValue) {
                Py_DECREF (pValue);
		/* Fixme */
		return NULL;
	} else
		return NULL;
}

static void
plugin_python_class_init (EPluginPythonClass *class)
{
	GObjectClass *object_class;
	EPluginClass *plugin_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EPluginPythonPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = plugin_python_finalize;

	plugin_class = E_PLUGIN_CLASS (class);
	plugin_class->construct = plugin_python_construct;
	plugin_class->invoke = plugin_python_invoke;
	plugin_class->type = "python";
}

static void
plugin_python_init (EPluginPython *plugin_python)
{
	GHashTable *methods;

	methods = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	plugin_python->priv = E_PLUGIN_PYTHON_GET_PRIVATE (plugin_python);
	plugin_python->priv->methods = methods;
}

GType
e_plugin_python_get_type (void)
{
	return plugin_python_type;
}

void
e_plugin_python_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EPluginPythonClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) plugin_python_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EPluginPython),
		0,     /* n_preallocs */
		(GInstanceInitFunc) plugin_python_init,
		NULL   /* value_table */
	};

	plugin_python_type = g_type_module_register_type (
		type_module, E_TYPE_PLUGIN,
		"EPluginPython", &type_info, 0);

	/* TODO Does this mean I can cache the instance of pyobjects? */
	Py_Initialize ();
}
