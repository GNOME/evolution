/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <glib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <gal/util/e-xml-utils.h>
#include <gal/util/e-util.h>
#include "e-meta.h"

static GObjectClass *e_meta_parent_class;

struct _meta_data {
	struct _meta_data *next;

	char *md_key;
	char *md_value;
};

struct _EMetaPrivate {
	char *path;
	struct _meta_data *data;
	gulong sync_id;
	/* if set, we wont try and save/etc */
	unsigned int deleted:1;
};

static int meta_save(EMeta *em)
{
	struct _EMetaPrivate *p = em->priv;
	xmlDocPtr doc;
	xmlNodePtr root, work;
	struct _meta_data *md;
	int res;
	char *dir;
	struct stat st;

	if (p->deleted)
		return 0;

	/* since we can, build the full path if we need to */
	dir = g_path_get_dirname(p->path);
	if (stat(dir, &st) == -1) {
		e_mkdir_hier(dir, 0777);
		g_free(dir);
	}

	/* of course, saving in xml is overkill, but everyone loves this shit ... */

	doc = xmlNewDoc("1.0");
	root = xmlNewDocNode(doc, NULL, "e-meta-data", NULL);
	xmlDocSetRootElement(doc, root);

	md = p->data;
	while (md) {
		work = xmlNewChild(root, NULL, "item", NULL);
		xmlSetProp(work, "name", md->md_key);
		xmlSetProp(work, "value", md->md_value);
		md = md->next;
	}

	res = e_xml_save_file(p->path, doc);
	if (res != 0)
		g_warning("Could not save folder meta-data `%s': %s", p->path, g_strerror(errno));

	xmlFreeDoc(doc);

	return res;
}

static int meta_load(EMeta *em)
{
	struct _EMetaPrivate *p = em->priv;
	struct _meta_data *tail, *md;
	xmlDocPtr doc = NULL;
	xmlNodePtr root, work;
	char *name, *val;
	struct stat st;
	
	if (stat (p->path, &st) == -1 || !S_ISREG (st.st_mode))
		return -1;
	
	doc = xmlParseFile(p->path);
	if (doc == NULL)
		return -1;

	root = xmlDocGetRootElement(doc);
	if (root == NULL || strcmp(root->name, "e-meta-data")) {
		xmlFreeDoc(doc);
		errno = EINVAL;
		return -1;
	}

	work = root->children;
	tail = (struct _meta_data *)&p->data;
	while (work) {
		if (strcmp(work->name, "item") == 0) {
			name = xmlGetProp(work, "name");
			val = xmlGetProp(work, "value");
			if (name && val) {
				md = g_malloc(sizeof(*md));
				md->md_key = g_strdup(name);
				md->md_value = g_strdup(val);
				md->next = NULL;
				tail->next = md;
				tail = md;
			}
			if (name)
				xmlFree(name);
			if (val)
				xmlFree(val);
		}
		work = work->next;
	}

	xmlFreeDoc(doc);

	return 0;
}

static struct _meta_data *meta_find(EMeta *em, const char *key, struct _meta_data **mpp)
{
	struct _meta_data *mp = (struct _meta_data *)&em->priv->data;
	struct _meta_data *md = mp->next;

	while (md && strcmp(md->md_key, key) != 0) {
		mp = md;
		md = md->next;
	}

	*mpp = mp;

	return md;
}

static void meta_free(struct _meta_data *md)
{
	g_free(md->md_key);
	g_free(md->md_value);
	g_free(md);
}

static void
e_meta_init (EMeta *em)
{
	em->priv = g_malloc0(sizeof(*em->priv));
}

static void
e_meta_finalise(GObject *crap)
{
	EMeta *em = (EMeta *)crap;
	struct _EMetaPrivate *p = em->priv;
	struct _meta_data *md, *mn;

	if (p->sync_id != 0)
		e_meta_sync(em);

	md = p->data;
	while (md) {
		mn = md->next;
		meta_free(md);
		md = mn;
	}

	g_free(p->path);
	g_free(p);
	e_meta_parent_class->finalize((GObject *)em);
}

static void
e_meta_class_init (EMetaClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	e_meta_parent_class = g_type_class_ref (G_TYPE_OBJECT);

	((GObjectClass *)klass)->finalize = e_meta_finalise;
}

static GTypeInfo e_meta_type_info = {
	sizeof (EMetaClass),
	NULL, /* base_class_init */
	NULL, /* base_class_finalize */
	(GClassInitFunc)  e_meta_class_init,
	NULL, /* class_finalize */
	NULL, /* class_data */
	sizeof (EMeta),
	0,    /* n_preallocs */
	(GInstanceInitFunc) e_meta_init
};

static GType e_meta_type;

GType
e_meta_get_type (void)
{
	return e_meta_type?e_meta_type:(e_meta_type = g_type_register_static (G_TYPE_OBJECT, "EMeta", &e_meta_type_info, 0));
}

/**
 * e_meta_new:
 * @path: full path to meta-data storage object.
 * 
 * Create a new meta-data storage object.  Any existing meta-data stored for
 * this key will be loaded.
 * 
 * Return value: 
 **/
EMeta *
e_meta_new(const char *path)
{
	EMeta *em;

	em = g_object_new(e_meta_get_type(), NULL);
	em->priv->path = g_strdup(path);
	meta_load(em);

	return em;
}

static gboolean
meta_flush(EMeta *em)
{
	em->priv->sync_id = 0;
	meta_save(em);

	return FALSE;
}

/* returns TRUE if the value changed */
static int meta_set(EMeta *em, const char *key, const char *val)
{
	struct _EMetaPrivate *p = em->priv;
	struct _meta_data *md, *mp;

	md = meta_find(em, key, &mp);
	if (md == NULL) {
		/* already unset / or new case */
		if (val == NULL)
			return FALSE;
		md = g_malloc0(sizeof(*md));
		md->md_key = g_strdup(key);
		md->next = p->data;
		p->data = md;
	} else if (val == NULL) {
		/* unset case */
		mp->next = md->next;
		meta_free(md);
		return TRUE;
	} else if (strcmp(md->md_value, val) == 0) {
		/* unchanged value */
		return FALSE;
	} else {
		/* changed value */
		g_free(md->md_value);
	}
	md->md_value = g_strdup(val);

	return TRUE;
}

/* get a value, returns NULL if it doesn't exist */
static const char *meta_get(EMeta *em, const char *key)
{
	struct _meta_data *md, *mp;

	md = meta_find(em, key, &mp);

	return md?md->md_value:NULL;
}

/**
 * e_meta_set:
 * @em: 
 * @key: 
 * @...: value, key, value, ..., NULL.
 * 
 * Set any number of meta-data key-value pairs.
 * Unset a key by passing a value of NULL.
 *
 * If the meta-data set changes as a result of this
 * call, then a sync will be implicitly queued for
 * a later time.
 **/
void
e_meta_set(EMeta *em, const char *key, ...)
{
	struct _EMetaPrivate *p = em->priv;
	const char *val;
	va_list ap;
	int changed = FALSE;

	va_start(ap, key);
	while (key != NULL) {
		val = va_arg(ap, const char *);
		changed = meta_set(em, key, val);
		key = va_arg(ap, const char *);
	}
	va_end(ap);

	/* todo: could do changed events ? */

	if (changed && p->sync_id == 0)
		p->sync_id = g_timeout_add(2000, (GSourceFunc)meta_flush, em);
}

/**
 * e_meta_get:
 * @em: 
 * @key: 
 * @...: value, key, value, ..., NULL.
 * 
 * Get any number of meta-data key-value pairs.
 **/
void
e_meta_get(EMeta *em, const char *key, ...)
{
	const char **valp;
	va_list ap;

	va_start(ap, key);
	while (key) {
		valp = va_arg(ap, const char **);
		*valp = meta_get(em, key);
		key = va_arg(ap, const char *);
	}
	va_end(ap);
}

/**
 * e_meta_get_bool:
 * @em: 
 * @key: 
 * @def: 
 * 
 * Get a boolean value at @key, with a default fallback @def.
 *
 * If the default value is used, then it will become the persistent
 * new value for the key.
 * 
 * Return value: The value of the key, or if the key was not
 * previously set, then the new value of the key, @def.
 **/
gboolean
e_meta_get_bool(EMeta *em, const char *key, gboolean def)
{
	const char *v;

	v = meta_get(em, key);
	/* this forces the value to become 'static' from first use */
	if (v == NULL) {
		e_meta_set_bool(em, key, def);
		return def;
	}

	return atoi(v);
}

/**
 * e_meta_set_bool:
 * @em: 
 * @key: 
 * @val: 
 * 
 * Helper to set a boolean value.  Boolean TRUE is mapped to
 * the string "1", FALSE to "0".
 **/
void
e_meta_set_bool(EMeta *em, const char *key, gboolean val)
{
	e_meta_set(em, key, val?"1":"0", NULL);
}

/**
 * e_meta_sync:
 * @em: 
 * 
 * Force an explicit and immediate sync of the meta-data to disk.
 *
 * This is not normally required unless part of transactional
 * processing, as updates will always be flushed to disk automatically.
 * 
 * Return value: 0 on success.
 **/
int
e_meta_sync(EMeta *em)
{
	struct _EMetaPrivate *p = em->priv;

	if (p->sync_id != 0) {
		g_source_remove(p->sync_id);
		p->sync_id = 0;
	}

	return meta_save(em);
}

static GHashTable *e_meta_table;

static char *meta_filename(const char *base, const char *key)
{
	const char *p;
	char *keyp, *o, c;

	p = key;
	o = keyp = alloca(strlen(key)+8);

	while ( (c = *p++) ) {
		if (c == '/')
			c = '_';
		*o++ = c;
	}
	strcpy(o, ".emeta");
	o = g_build_filename(base, keyp, NULL);

	return o;
}

static void
meta_weak_notify(char *path, void *o)
{
	g_hash_table_remove(e_meta_table, path);
	g_free(path);
}

/**
 * e_meta_data_lookup:
 * @base: Base storage directory.
 * @key: key for file.
 * 
 * Lookup a meta-data object from a storage directory.
 * 
 * Return value: The metadata object.
 **/
EMeta *e_meta_data_find(const char *base, const char *key)
{
	EMeta *em;
	char *path;

	if (e_meta_table == NULL)
		e_meta_table = g_hash_table_new(g_str_hash, g_str_equal);

	path = meta_filename(base, key);
	em = g_hash_table_lookup(e_meta_table, path);
	if (em) {
		g_free(path);
		g_object_ref(em);
		return em;
	}

	em = e_meta_new(path);
	g_hash_table_insert(e_meta_table, path, em);
	g_object_weak_ref((GObject *)em, (GWeakNotify)meta_weak_notify, path);

	return em;
}

/**
 * e_meta_data_delete:
 * @base:
 * @key: 
 * 
 * Delete a key from storage.  If the key is still cached, it will be
 * marked as deleted, and will not be saved from then on.
 **/
void e_meta_data_delete(const char *base, const char *key)
{
	EMeta *em;
	char *path;

	path = meta_filename(base, key);

	if (e_meta_table && (em = g_hash_table_lookup(e_meta_table, path))) {
		if (em->priv->sync_id) {
			g_source_remove(em->priv->sync_id);
			em->priv->sync_id = 0;
		}
		em->priv->deleted = TRUE;
	}

	unlink(path);
	g_free(path);
}
