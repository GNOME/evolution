/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
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

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <gconf/gconf-client.h>

#include "e-uid.h"

#include "e-signature.h"


static void e_signature_class_init (ESignatureClass *klass);
static void e_signature_init (ESignature *sig, ESignatureClass *klass);
static void e_signature_finalize (GObject *object);


static GObjectClass *parent_class = NULL;


GType
e_signature_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		GTypeInfo type_info = {
			sizeof (ESignatureClass),
			NULL, NULL,
			(GClassInitFunc) e_signature_class_init,
			NULL, NULL,
			sizeof (ESignature),
			0,
			(GInstanceInitFunc) e_signature_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "ESignature", &type_info, 0);
	}
	
	return type;
}

static void
e_signature_class_init (ESignatureClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	/* virtual method override */
	object_class->finalize = e_signature_finalize;
}

static void
e_signature_init (ESignature *sig, ESignatureClass *klass)
{
	;
}

static void
e_signature_finalize (GObject *object)
{
	ESignature *sig = (ESignature *) object;
	
	g_free (sig->uid);
	g_free (sig->name);
	g_free (sig->filename);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * e_signature_new:
 *
 * Returns a new signature which can be filled in and
 * added to an #ESignatureList.
 **/
ESignature *
e_signature_new (void)
{
	ESignature *signature;
	
	signature = g_object_new (E_TYPE_SIGNATURE, NULL);
	signature->uid = e_uid_new ();
	
	return signature;
}


/**
 * e_signature_new_from_xml:
 * @xml: an XML signature description
 *
 * Return value: a new #ESignature based on the data in @xml, or %NULL
 * if @xml could not be parsed as valid signature data.
 **/
ESignature *
e_signature_new_from_xml (const char *xml)
{
	ESignature *signature;
	
	signature = g_object_new (E_TYPE_SIGNATURE, NULL);
	if (!e_signature_set_from_xml (signature, xml)) {
		g_object_unref (signature);
		return NULL;
	}
	
	return signature;
}


static gboolean
xml_set_bool (xmlNodePtr node, const char *name, gboolean *val)
{
	gboolean bool;
	char *buf;
	
	if ((buf = xmlGetProp (node, name))) {
		bool = (!strcmp (buf, "true") || !strcmp (buf, "yes"));
		xmlFree (buf);
		
		if (bool != *val) {
			*val = bool;
			return TRUE;
		}
	}
	
	return FALSE;
}

static gboolean
xml_set_prop (xmlNodePtr node, const char *name, char **val)
{
	char *buf, *new_val;
	
	buf = xmlGetProp (node, name);
	new_val = g_strdup (buf);
	xmlFree (buf);
	
	/* We can use strcmp here whether the value is UTF8 or
	 * not, since we only care if the bytes changed.
	 */
	if (!*val || strcmp (*val, new_val)) {
		g_free (*val);
		*val = new_val;
		return TRUE;
	} else {
		g_free (new_val);
		return FALSE;
	}
}

static gboolean
xml_set_content (xmlNodePtr node, char **val)
{
	char *buf, *new_val;
	
	buf = xmlNodeGetContent (node);
        new_val = g_strdup (buf);
	xmlFree (buf);
	
	/* We can use strcmp here whether the value is UTF8 or
	 * not, since we only care if the bytes changed.
	 */
	if (!*val || strcmp (*val, new_val)) {
		g_free (*val);
		*val = new_val;
		return TRUE;
	} else {
		g_free (new_val);
		return FALSE;
	}
}


/**
 * e_signature_uid_from_xml:
 * @xml: an XML signature description
 *
 * Return value: the permanent UID of the signature described by @xml
 * (or %NULL if @xml could not be parsed or did not contain a uid).
 * The caller must free this string.
 **/
char *
e_signature_uid_from_xml (const char *xml)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	char *uid = NULL;
	
	if (!(doc = xmlParseDoc ((char *) xml)))
		return NULL;
	
	node = doc->children;
	if (strcmp (node->name, "signature") != 0) {
		xmlFreeDoc (doc);
		return NULL;
	}
	
	xml_set_prop (node, "uid", &uid);
	xmlFreeDoc (doc);
	
	return uid;
}


/**
 * e_signature_set_from_xml:
 * @signature: an #ESignature
 * @xml: an XML signature description.
 *
 * Changes @signature to match @xml.
 *
 * Returns %TRUE if the signature was loaded or %FALSE otherwise.
 **/
gboolean
e_signature_set_from_xml (ESignature *signature, const char *xml)
{
	gboolean changed = FALSE;
	xmlNodePtr node, cur;
	xmlDocPtr doc;
	gboolean bool;
	char *buf;
	
	if (!(doc = xmlParseDoc ((char *) xml)))
		return FALSE;
	
	node = doc->children;
	if (strcmp (node->name, "signature") != 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	if (!signature->uid)
		xml_set_prop (node, "uid", &signature->uid);
	
	changed |= xml_set_prop (node, "name", &signature->name);
	changed |= xml_set_bool (node, "auto", &signature->autogen);
	
	if (signature->autogen) {
		/* we're done */
		g_free (signature->filename);
		signature->filename = NULL;
		signature->script = FALSE;
		signature->html = FALSE;
		xmlFreeDoc (doc);
		
		return changed;
	}
	
	buf = NULL;
	xml_set_prop (node, "format", &buf);
	if (buf && !strcmp (buf, "text/html"))
		bool = TRUE;
	else
		bool = FALSE;
	g_free (buf);
	
	if (signature->html != bool) {
		signature->html = bool;
		changed = TRUE;
	}
	
	cur = node->children;
	while (cur) {
		if (!strcmp (cur->name, "filename")) {
			changed |= xml_set_content (cur, &signature->filename);
			changed |= xml_set_bool (cur, "script", &signature->script);
			break;
		} else if (!strcmp (cur->name, "script")) {
			/* this is for handling 1.4 signature script definitions */
			changed |= xml_set_content (cur, &signature->filename);
			if (!signature->script) {
				signature->script = TRUE;
				changed = TRUE;
			}
			break;
		}
		
		cur = cur->next;
	}
	
	xmlFreeDoc (doc);
	
	return changed;
}


/**
 * e_signature_to_xml:
 * @signature: an #ESignature
 *
 * Return value: an XML representation of @signature, which the caller
 * must free.
 **/
char *
e_signature_to_xml (ESignature *signature)
{
	char *xmlbuf, *tmp;
	xmlNodePtr root, node;
	xmlDocPtr doc;
	int n;
	
	doc = xmlNewDoc ("1.0");
	
	root = xmlNewDocNode (doc, NULL, "signature", NULL);
	xmlDocSetRootElement (doc, root);
	
	xmlSetProp (root, "name", signature->name);
	xmlSetProp (root, "uid", signature->uid);
	xmlSetProp (root, "auto", signature->autogen ? "true" : "false");
	
	if (!signature->autogen) {
		xmlSetProp (root, "format", signature->html ? "text/html" : "text/plain");
		
		if (signature->filename) {
			node = xmlNewTextChild (root, NULL, "filename", signature->filename);
			if (signature->script)
				xmlSetProp (node, "script", "true");
		}
	} else {
		/* this is to make Evolution-1.4 and older 1.5 versions happy */
		xmlSetProp (root, "format", "text/html");
	}
	
	xmlDocDumpMemory (doc, (xmlChar **) &xmlbuf, &n);
	xmlFreeDoc (doc);
	
	/* remap to glib memory */
	tmp = g_malloc (n + 1);
	memcpy (tmp, xmlbuf, n);
	tmp[n] = '\0';
	xmlFree (xmlbuf);
	
	return tmp;
}

