/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <camel/camel.h>
#include <camel/camel-session.h>
#include <camel/camel-file-utils.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#define EM_MIGRATE_SESSION_TYPE     (em_migrate_session_get_type ())
#define EM_MIGRATE_SESSION(obj)     (CAMEL_CHECK_CAST((obj), EM_MIGRATE_SESSION_TYPE, EMMigrateSession))
#define EM_MIGRATE_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_MIGRATE_SESSION_TYPE, EMMigrateSessionClass))
#define EM_MIGRATE_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), EM_MIGRATE_SESSION_TYPE))

typedef struct _EMMigrateSession {
	CamelSession parent_object;
	
	CamelStore *store;   /* new folder tree store */
	char *srcdir;        /* old folder tree path */
} EMMigrateSession;

typedef struct _EMMigrateSessionClass {
	CamelSessionClass parent_class;
	
} EMMigrateSessionClass;

CamelType em_migrate_session_get_type (void);
CamelSession *em_migrate_session_new (const char *path);

static void
class_init (EMMigrateSessionClass *klass)
{
	;
}

CamelType
em_migrate_session_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_session_get_type (),
			"EMMigrateSession",
			sizeof (EMMigrateSession),
			sizeof (EMMigrateSessionClass),
			(CamelObjectClassInitFunc) class_init,
			NULL,
			NULL,
			NULL);
	}
	
	return type;
}

CamelSession *
em_migrate_session_new (const char *path)
{
	CamelSession *session;
	
	session = CAMEL_SESSION (camel_object_new (EM_MIGRATE_SESSION_TYPE));
	
	camel_session_construct (session, path);
	
	return session;
}

static gboolean
is_mail_folder (const char *metadata)
{
	struct stat st;
	xmlNodePtr node;
	xmlDocPtr doc;
	char *type;
	
	if (stat (metadata, &st) == -1 || !S_ISREG (st.st_mode))
		return FALSE;
	
	if (!(doc = xmlParseFile (metadata))) {
		g_warning ("Cannot parse `%s'", metadata);
		return FALSE;
	}
	
	if (!(node = xmlDocGetRootElement (doc))) {
		g_warning ("`%s' corrupt: document contains no root node", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	if (!node->name || strcmp (node->name, "efolder") != 0) {
		g_warning ("`%s' corrupt: root node is not 'efolder'", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "type")) {
			type = xmlNodeGetContent (node);
			if (!strcmp (type, "mail")) {
				xmlFreeDoc (doc);
				xmlFree (type);
				
				return TRUE;
			}
			
			xmlFree (type);
			
			break;
		}
		
		node = node->next;
	}
	
	xmlFreeDoc (doc);
	
	return FALSE;
}

static CamelStore *
get_local_store (CamelSession *session, const char *dirname, const char *metadata, char **namep, int *index, CamelException *ex)
{
	char *protocol, *name, *buf;
	CamelStore *store;
	struct stat st;
	xmlNodePtr node;
	xmlDocPtr doc;
	
	if (stat (metadata, &st) == -1 || !S_ISREG (st.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "`%s' is not a regular file", metadata);
		return NULL;
	}
	
	if (!(doc = xmlParseFile (metadata))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "cannot parse `%s'", metadata);
		return NULL;
	}
	
	if (!(node = xmlDocGetRootElement (doc)) || strcmp (node->name, "folderinfo") != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "`%s' is malformed", metadata);
		xmlFreeDoc (doc);
		return NULL;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "folder")) {
			protocol = xmlGetProp (node, "type");
			name = xmlGetProp (node, "name");
			buf = xmlGetProp (node, "index");
			if (buf != NULL) {
				*index = atoi (buf);
				xmlFree (buf);
			} else {
				*index = 0;
			}
			
			xmlFreeDoc (doc);
			
			buf = g_strdup_printf ("%s:%s", protocol, dirname);
			xmlFree (protocol);
			
			if ((store = camel_session_get_store (session, buf, ex)))
				*namep = g_strdup (name);
			else
				*namep = NULL;
			
			xmlFree (name);
			
			return store;
		}
		
		node = node->next;
	}
	
	xmlFreeDoc (doc);
	
	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "`%s' does not contain needed info", metadata);
	
	return NULL;
}

static void
em_migrate_dir (EMMigrateSession *session, const char *dirname, const char *full_name)
{
	guint32 flags = CAMEL_STORE_FOLDER_CREATE;
	CamelFolder *old_folder, *new_folder;
	CamelStore *local_store;
	struct dirent *dent;
	CamelException ex;
	char *path, *name;
	GPtrArray *uids;
	struct stat st;
	int index;
	DIR *dir;
	
	path = g_strdup_printf ("%s/folder-metadata.xml", dirname);
	if (!is_mail_folder (path)) {
		g_free (path);
		goto try_subdirs;
	}
	
	g_free (path);
	
	camel_exception_init (&ex);
	
	/* get old store & folder */
	path = g_strdup_printf ("%s/local-metadata.xml", dirname);
	if (!(local_store = get_local_store ((CamelSession *) session, dirname, path, &name, &index, &ex))) {
		g_warning ("error opening old store for `%s': %s", full_name, ex.desc);
		camel_exception_clear (&ex);
		g_free (path);
		return;
	}
	
	g_free (path);
	
	if (!(old_folder = camel_store_get_folder (local_store, name, 0, &ex))) {
		g_warning ("error opening old folder `%s': %s", full_name, ex.desc);
		camel_object_unref (local_store);
		camel_exception_clear (&ex);
		g_free (name);
		return;
	}
	
	g_free (name);
	
	flags |= (index ? CAMEL_STORE_FOLDER_BODY_INDEX : 0);
	if (!(new_folder = camel_store_get_folder (session->store, full_name, flags, &ex))) {
		g_warning ("error creating new mbox folder `%s': %s", full_name, ex.desc);
		camel_object_unref (local_store);
		camel_object_unref (old_folder);
		camel_exception_clear (&ex);
		return;
	}
	
	uids = camel_folder_get_uids (old_folder);
	camel_folder_transfer_messages_to (old_folder, uids, new_folder, NULL, FALSE, &ex);
	camel_folder_free_uids (old_folder, uids);
	
	if (camel_exception_is_set (&ex)) {
		g_warning ("error migrating folder `%s': %s", full_name, ex.desc);
		camel_object_unref (local_store);
		camel_object_unref (old_folder);
		camel_object_unref (new_folder);
		camel_exception_clear (&ex);
		return;
	}
	
	/*camel_object_unref (local_store);*/
	camel_object_unref (old_folder);
	camel_object_unref (new_folder);
	
 try_subdirs:
	
	path = g_strdup_printf ("%s/subfolders", dirname);
	if (stat (path, &st) == -1 || !S_ISDIR (st.st_mode)) {
		g_free (path);
		return;
	}
	
	if (!(dir = opendir (path))) {
		g_warning ("cannot open `%s': %s", path, strerror (errno));
		g_free (path);
		return;
	}
	
	while ((dent = readdir (dir))) {
		char *full_path;
		
		if (dent->d_name[0] == '.')
			continue;
		
		full_path = g_strdup_printf ("%s/%s", path, dent->d_name);
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}
		
		name = g_strdup_printf ("%s/%s", full_name, dent->d_name);
		em_migrate_dir (session, full_path, name);
		g_free (full_path);
		g_free (name);
	}
	
	closedir (dir);
	
	g_free (path);
}

static void
em_migrate (EMMigrateSession *session)
{
	struct dirent *dent;
	struct stat st;
	DIR *dir;
	
	if (!(dir = opendir (session->srcdir))) {
		g_warning ("cannot open `%s': %s", session->srcdir, strerror (errno));
		return;
	}
	
	while ((dent = readdir (dir))) {
		char *full_path;
		
		if (dent->d_name[0] == '.')
			continue;
		
		full_path = g_strdup_printf ("%s/%s", session->srcdir, dent->d_name);
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}
		
		em_migrate_dir (session, full_path, dent->d_name);
		g_free (full_path);
	}
	
	closedir (dir);
}

int main (int argc, char **argv)
{
	EMMigrateSession *session;
	CamelException ex;
	struct stat st;
	char *path;
	
	g_thread_init (NULL);
	
	path = g_strdup_printf ("%s/.evolution/mail", g_get_home_dir ());
	if (stat (path, &st) == -1) {
		if (errno != ENOENT || camel_mkdir (path, 0777) == -1) {
			g_warning ("Failed to create directory `%s': %s", path, strerror (errno));
			g_free (path);
			return -1;
		}
	}
	
	camel_init (path, TRUE);
	session = (EMMigrateSession *) em_migrate_session_new (path);
	g_free (path);
	
	session->srcdir = g_strdup_printf ("%s/evolution/local", g_get_home_dir ());
	
	path = g_strdup_printf ("mbox:%s/.evolution/mail/local", g_get_home_dir ());
	if (stat (path + 5, &st) == -1) {
		if (errno != ENOENT || camel_mkdir (path + 5, 0777) == -1) {
			g_warning ("Failed to create directory `%s': %s", path + 5, strerror (errno));
			g_free (session->srcdir);
			camel_object_unref (session);
			g_free (path);
			return -1;
		}
	}
	
	camel_exception_init (&ex);
	if (!(session->store = camel_session_get_store ((CamelSession *) session, path, &ex))) {
		g_warning ("Failed to open store for `%s': %s", path, ex.desc);
		g_free (session->srcdir);
		camel_object_unref (session);
		camel_exception_clear (&ex);
		g_free (path);
		return -1;
	}
	g_free (path);
	
	em_migrate (session);
	
	camel_object_unref (session->store);
	g_free (session->srcdir);
	
	camel_object_unref (session);
	
	return 0;
}
