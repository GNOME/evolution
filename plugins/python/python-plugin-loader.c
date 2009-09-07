/*
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
 * Authors:
 *		Johnny Jacob <jjohnny@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <Python.h>

#include <sys/types.h>
#include <string.h>

#include "python-plugin-loader.h"

#define d(x)

static gpointer epp_parent_class;

typedef struct _EPluginPythonPrivate {
        PyObject *pModule;
        PyObject *pClass;
	PyObject *pFunc;
	PyObject *pDict;
	GHashTable *methods;
} EPluginPythonPrivate;

#define epp ((EPluginPython *)ep)

gpointer  load_plugin_type_register_function (gpointer a, gpointer b);

static gchar *
get_xml_prop(xmlNodePtr node, const gchar *id)
{
	gchar *p = xmlGetProp(node, id);
	gchar *out = NULL;

	if (p) {
		out = g_strdup(p);
		xmlFree(p);
	}

	return out;
}

static gpointer
epp_invoke(EPlugin *ep, const gchar *name, gpointer data)
{
	EPluginPythonPrivate *p = epp->priv;
	PyObject *pModuleName, *pFunc;
	PyObject *pInstance, *pValue = NULL;

	/* we need to do this every time since we may be called from any thread for some uses */
	Py_Initialize();

	if (p->pModule == NULL) {
		pModuleName = PyString_FromString(epp->module_name);

		PyRun_SimpleString(g_strdup_printf ("import sys; sys.path.insert(0, '%s')", epp->location));

		p->pModule = PyImport_Import(pModuleName);

		Py_DECREF(pModuleName); //Free

		if (p->pModule == NULL) {
			PyErr_Print();
			g_warning("can't load python module '%s'", epp->location);
			return NULL;
		}

		p->pDict = PyModule_GetDict(p->pModule);

		if (epp->pClass) {
			p->pClass = PyDict_GetItemString(p->pDict, epp->pClass);
		}
	}

	if (p->pClass) {

		if (PyCallable_Check(p->pClass))
			pInstance = PyObject_CallObject(p->pClass, NULL);

		pValue = PyObject_CallMethod(pInstance, (gchar *) name, NULL);

	} else {

		pFunc = PyDict_GetItemString(p->pDict, name);

		if (pFunc && PyCallable_Check(pFunc))
			pValue = PyObject_CallObject(pFunc, NULL);
		else
			PyErr_Print();
	}

	if (pValue) {
		d(printf("%s(%d):%s: Result of call: %ld \n", __FILE__, __LINE__, __PRETTY_FUNCTION__, PyInt_AsLong(pValue)));
                Py_DECREF(pValue);
		/* Fixme */
		return NULL;
	} else
		return NULL;
}

static gint
epp_construct(EPlugin *ep, xmlNodePtr root)
{
	if (((EPluginClass *)epp_parent_class)->construct(ep, root) == -1)
		return -1;

	epp->location = get_xml_prop(root, "location");
	epp->module_name = get_xml_prop (root, "module_name");
	epp->pClass = get_xml_prop(root, "pClass");

	if (epp->location == NULL)
		return -1;

	return 0;
}

static void
epp_finalise(GObject *o)
{
	EPlugin *ep = (EPlugin *)o;
	EPluginPythonPrivate *p = epp->priv;

	g_free(epp->location);
	g_free(epp->module_name);
	g_free(epp->pClass);

	g_hash_table_destroy(p->methods);

	g_free(epp->priv);

	((GObjectClass *)epp_parent_class)->finalize(o);
}

static void
epp_class_init(EPluginClass *klass)
{
	((GObjectClass *)klass)->finalize = epp_finalise;
	klass->construct = epp_construct;
	klass->invoke = epp_invoke;
	klass->type = "python";
}

static void
epp_init(GObject *o)
{
	EPlugin *ep = (EPlugin *)o;

	epp->priv = g_malloc0(sizeof(*epp->priv));
	epp->priv->methods = g_hash_table_new_full(
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);
}

gpointer
load_plugin_type_register_function (gpointer a, gpointer b)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EPluginPythonClass), NULL, NULL, (GClassInitFunc) epp_class_init, NULL, NULL,
			sizeof(EPluginPython), 0, (GInstanceInitFunc) epp_init,
		};

		epp_parent_class = g_type_class_ref(e_plugin_get_type());
		type = g_type_register_static(e_plugin_get_type(), "EPluginPython", &info, 0);
		e_plugin_register_type (type);

		d(printf("\nType EPluginPython registered from the python-plugin-loader\n"));

		Py_Initialize(); //TODO : Does this mean i can cache the instance of pyobjects ?
	}

	return GUINT_TO_POINTER(type);
}
