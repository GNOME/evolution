/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@novell.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <camel/camel-store.h>
#include <camel/camel-file-utils.h>

#include "camel-imap4-utils.h"
#include "camel-imap4-store-summary.h"


#define CAMEL_IMAP4_STORE_SUMMARY_VERSION_0 (0)
#define CAMEL_IMAP4_STORE_SUMMARY_VERSION (0)

static void camel_imap4_store_summary_class_init (CamelIMAP4StoreSummaryClass *klass);
static void camel_imap4_store_summary_init (CamelIMAP4StoreSummary *obj);
static void camel_imap4_store_summary_finalize (CamelObject *obj);

static int summary_header_load (CamelStoreSummary *s, FILE *in);
static int summary_header_save (CamelStoreSummary *s, FILE *out);

static CamelStoreInfo *store_info_load (CamelStoreSummary *s, FILE *in);
static int store_info_save (CamelStoreSummary *s, FILE *out, CamelStoreInfo *info);
static void store_info_free (CamelStoreSummary *s, CamelStoreInfo *info);


static CamelStoreSummaryClass *parent_class = NULL;


CamelType
camel_imap4_store_summary_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_store_summary_get_type (),
					    "CamelIMAP4StoreSummary",
					    sizeof (CamelIMAP4StoreSummary),
					    sizeof (CamelIMAP4StoreSummaryClass),
					    (CamelObjectClassInitFunc) camel_imap4_store_summary_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_imap4_store_summary_init,
					    (CamelObjectFinalizeFunc) camel_imap4_store_summary_finalize);
	}
	
	return type;
}


static void
camel_imap4_store_summary_class_init (CamelIMAP4StoreSummaryClass *klass)
{
	CamelStoreSummaryClass *ssklass = (CamelStoreSummaryClass *) klass;
	
	parent_class = (CamelStoreSummaryClass *) camel_store_summary_get_type ();
	
	ssklass->summary_header_load = summary_header_load;
	ssklass->summary_header_save = summary_header_save;
	
	ssklass->store_info_load = store_info_load;
	ssklass->store_info_save = store_info_save;
	ssklass->store_info_free = store_info_free;
}

static void
camel_imap4_store_summary_init (CamelIMAP4StoreSummary *s)
{
	((CamelStoreSummary *) s)->store_info_size = sizeof (CamelIMAP4StoreInfo);
	s->version = CAMEL_IMAP4_STORE_SUMMARY_VERSION;
	s->namespaces = NULL;
}

static void
camel_imap4_store_summary_finalize (CamelObject *obj)
{
	CamelIMAP4StoreSummary *s = (CamelIMAP4StoreSummary *) obj;
	
	if (s->namespaces)
		camel_imap4_namespace_list_free (s->namespaces);
}


static CamelIMAP4NamespaceList *
load_namespaces (FILE *in)
{
	CamelIMAP4Namespace *ns, *tail;
	CamelIMAP4NamespaceList *nsl;
	guint32 i, j, n;
	
	nsl = g_malloc (sizeof (CamelIMAP4NamespaceList));
	nsl->personal = NULL;
	nsl->shared = NULL;
	nsl->other = NULL;
	
	for (j = 0; j < 3; j++) {
		switch (j) {
		case 0:
			tail = (CamelIMAP4Namespace *) &nsl->personal;
			break;
		case 1:
			tail = (CamelIMAP4Namespace *) &nsl->shared;
			break;
		case 2:
			tail = (CamelIMAP4Namespace *) &nsl->other;
			break;
		}
		
		if (camel_file_util_decode_fixed_int32 (in, &n) == -1)
			goto exception;
		
		for (i = 0; i < n; i++) {
			guint32 sep;
			char *path;
			
			if (camel_file_util_decode_string (in, &path) == -1)
				goto exception;
			
			if (camel_file_util_decode_uint32 (in, &sep) == -1) {
				g_free (path);
				goto exception;
			}
			
			tail->next = ns = g_malloc (sizeof (CamelIMAP4Namespace));
			ns->sep = sep & 0xff;
			ns->path = path;
			ns->next = NULL;
			tail = ns;
		}
	}
	
	return nsl;
	
 exception:
	
	camel_imap4_namespace_list_free (nsl);
	
	return NULL;
}

static int
summary_header_load (CamelStoreSummary *s, FILE *in)
{
	CamelIMAP4StoreSummary *is = (CamelIMAP4StoreSummary *) s;
	guint32 version, capa;
	
	if (parent_class->summary_header_load (s, in) == -1)
		return -1;
	
	if (camel_file_util_decode_fixed_int32 (in, &version) == -1)
		return -1;
	
	is->version = version;
	if (version < CAMEL_IMAP4_STORE_SUMMARY_VERSION_0) {
		g_warning ("IMAP4 store summary header version too low");
		errno = EINVAL;
		return -1;
	}
	
	if (camel_file_util_decode_fixed_int32 (in, &capa) == -1)
		return -1;
	
	is->capa = capa;
	
	if (!(is->namespaces = load_namespaces (in)))
		return -1;
	
	return 0;
}

static int
save_namespaces (FILE *out, CamelIMAP4NamespaceList *nsl)
{
	CamelIMAP4Namespace *cur, *ns;
	guint32 i, n;
	
	for (i = 0; i < 3; i++) {
		switch (i) {
		case 0:
			cur = nsl->personal;
			break;
		case 1:
			cur = nsl->shared;
			break;
		case 2:
			cur = nsl->other;
			break;
		}
		
		for (ns = cur, n = 0; ns; n++)
			ns = ns->next;
		
		if (camel_file_util_encode_fixed_int32 (out, n) == -1)
			return -1;
		
		ns = cur;
		while (ns != NULL) {
			if (camel_file_util_encode_string (out, ns->path) == -1)
				return -1;
			
			if (camel_file_util_encode_uint32 (out, ns->sep) == -1)
				return -1;
			
			ns = ns->next;
		}
	}
	
	return 0;
}

static int
summary_header_save (CamelStoreSummary *s, FILE *out)
{
	CamelIMAP4StoreSummary *is = (CamelIMAP4StoreSummary *) s;
	
	if (parent_class->summary_header_save (s, out) == -1)
		return -1;
	
	if (camel_file_util_encode_fixed_int32 (out, is->version) == -1)
		return -1;
	
	if (camel_file_util_encode_fixed_int32 (out, is->capa) == -1)
		return -1;
	
	if (save_namespaces (out, is->namespaces) == -1)
		return -1;
	
	return 0;
}

static CamelStoreInfo *
store_info_load (CamelStoreSummary *s, FILE *in)
{
	return parent_class->store_info_load (s, in);
}

static int
store_info_save (CamelStoreSummary *s, FILE *out, CamelStoreInfo *info)
{
	return parent_class->store_info_save (s, out, info);
}

static void
store_info_free (CamelStoreSummary *s, CamelStoreInfo *info)
{
	parent_class->store_info_free (s, info);
}


/**
 * camel_imap4_store_summary_new:
 *
 * Create a new CamelIMAP4StoreSummary object.
 * 
 * Returns a new CamelIMAP4StoreSummary object.
 **/
CamelIMAP4StoreSummary *
camel_imap4_store_summary_new (void)
{
	return (CamelIMAP4StoreSummary *) camel_object_new (camel_imap4_store_summary_get_type ());
}


void
camel_imap4_store_summary_set_capabilities (CamelIMAP4StoreSummary *s, guint32 capa)
{
	s->capa = capa;
}


void
camel_imap4_store_summary_set_namespaces (CamelIMAP4StoreSummary *s, const CamelIMAP4NamespaceList *ns)
{
	if (s->namespaces)
		camel_imap4_namespace_list_free (s->namespaces);
	s->namespaces = camel_imap4_namespace_list_copy (ns);
}


void
camel_imap4_store_summary_note_info (CamelIMAP4StoreSummary *s, CamelFolderInfo *fi)
{
	CamelStoreSummary *ss = (CamelStoreSummary *) s;
	CamelStoreInfo *si;
	
	if ((si = camel_store_summary_path (ss, fi->full_name))) {
		if (fi->unread != -1) {
			si->unread = fi->unread;
			ss->flags |= CAMEL_STORE_SUMMARY_DIRTY;
		}
		
		if (fi->total != -1) {
			si->total = fi->total;
			ss->flags |= CAMEL_STORE_SUMMARY_DIRTY;
		}
		
		camel_store_summary_info_free (ss, si);
		return;
	}
	
	si = camel_store_summary_info_new (ss);
	si->path = g_strdup (fi->full_name);
	si->uri = g_strdup (fi->uri);
	si->flags = fi->flags;
	si->unread = fi->unread;
	si->total = fi->total;
	
	camel_store_summary_add (ss, si);
	
	/* FIXME: should this be recursive? */
}


void
camel_imap4_store_summary_unnote_info (CamelIMAP4StoreSummary *s, CamelFolderInfo *fi)
{
	CamelStoreSummary *ss = (CamelStoreSummary *) s;
	
	camel_store_summary_remove_path (ss, fi->full_name);
}


static CamelFolderInfo *
store_info_to_folder_info (CamelStoreSummary *s, CamelStoreInfo *si)
{
	CamelFolderInfo *fi;
	
	fi = g_malloc0 (sizeof (CamelFolderInfo));
	fi->full_name = g_strdup (camel_store_info_path (s, si));
	fi->name = g_strdup (camel_store_info_name (s, si));
	fi->uri = g_strdup (camel_store_info_uri (s, si));
	fi->flags = si->flags;
	fi->unread = si->unread;
	fi->total = si->total;
	
	return fi;
}

CamelFolderInfo *
camel_imap4_store_summary_get_folder_info (CamelIMAP4StoreSummary *s, const char *top, guint32 flags)
{
	CamelStoreSummary *ss = (CamelStoreSummary *) s;
	CamelFolderInfo *fi;
	GPtrArray *folders;
	CamelStoreInfo *si;
	size_t toplen, len;
	int i;
	
	toplen = strlen (top);
	folders = g_ptr_array_new ();
	
	for (i = 0; i < ss->folders->len; i++) {
		si = ss->folders->pdata[i];
		if (strncmp (si->path, top, toplen) != 0)
			continue;
		
		if (toplen > 0 && (len = strlen (si->path)) > toplen && si->path[toplen] != '/')
			continue;
		
		if (len == toplen) {
			/* found toplevel folder */
			g_ptr_array_add (folders, store_info_to_folder_info (ss, si));
			continue;
		}
		
		if ((flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) || !strchr (si->path + toplen + 1, '/'))
			g_ptr_array_add (folders, store_info_to_folder_info (ss, si));
	}
	
	fi = camel_folder_info_build (folders, top, '/', TRUE);
	g_ptr_array_free (folders, TRUE);
	
	return fi;
}
