/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * pas-backend-summary.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <netinet/in.h>

#include <gal/widgets/e-unicode.h>

#include "ebook/e-contact.h"
#include "pas-backend-summary.h"
#include "e-util/e-sexp.h"

static GObjectClass *parent_class;

struct _PASBackendSummaryPrivate {
	char *summary_path;
	FILE *fp;
	guint32 file_version;
	time_t mtime;
	gboolean upgraded;
	gboolean dirty;
	int flush_timeout_millis;
	int flush_timeout;
	GPtrArray *items;
	GHashTable *id_to_item;
	guint32 num_items; /* used only for loading */
#ifdef SUMMARY_STATS
	int size;
#endif
};

typedef struct {
	char *id;
	char *nickname;
	char *full_name;
	char *given_name;
	char *surname;
	char *file_as;
	char *email_1;
	char *email_2;
	char *email_3;
	gboolean wants_html;
	gboolean wants_html_set;
	gboolean list;
	gboolean list_show_addresses;
} PASBackendSummaryItem;

typedef struct {
	/* these lengths do *not* including the terminating \0, as
	   it's not stored on disk. */
	guint16 id_len;
	guint16 nickname_len;
	guint16 full_name_len; /* version 3.0 field */
	guint16 given_name_len;
	guint16 surname_len;
	guint16 file_as_len;
	guint16 email_1_len;
	guint16 email_2_len;
	guint16 email_3_len;
	guint8  wants_html;
	guint8  wants_html_set;
	guint8  list;
	guint8  list_show_addresses;
} PASBackendSummaryDiskItem;

typedef struct {
	guint32 file_version;
	guint32 num_items;
	guint32 summary_mtime; /* version 2.0 field */
} PASBackendSummaryHeader;

#define PAS_SUMMARY_MAGIC "PAS-SUMMARY"
#define PAS_SUMMARY_MAGIC_LEN 11

#define PAS_SUMMARY_FILE_VERSION_1_0 1000
#define PAS_SUMMARY_FILE_VERSION_2_0 2000
#define PAS_SUMMARY_FILE_VERSION_3_0 3000
#define PAS_SUMMARY_FILE_VERSION_4_0 4000

#define PAS_SUMMARY_FILE_VERSION PAS_SUMMARY_FILE_VERSION_4_0

static void
free_summary_item (PASBackendSummaryItem *item)
{
	g_free (item->id);
	g_free (item->nickname);
	g_free (item->full_name);
	g_free (item->given_name);
	g_free (item->surname);
	g_free (item->file_as);
	g_free (item->email_1);
	g_free (item->email_2);
	g_free (item->email_3);
	g_free (item);
}

static void
clear_items (PASBackendSummary *summary)
{
	int i;
	int num = summary->priv->items->len;
	for (i = 0; i < num; i++) {
		PASBackendSummaryItem *item = g_ptr_array_remove_index_fast (summary->priv->items, 0);
		g_hash_table_remove (summary->priv->id_to_item, item->id);
		free_summary_item (item);
	}
}

PASBackendSummary*
pas_backend_summary_new (const char *summary_path, int flush_timeout_millis)
{
	PASBackendSummary *summary = g_object_new (PAS_TYPE_BACKEND_SUMMARY, NULL);

	summary->priv->summary_path = g_strdup (summary_path);
	summary->priv->flush_timeout_millis = flush_timeout_millis;
	summary->priv->file_version = PAS_SUMMARY_FILE_VERSION_4_0;

	return summary;
}

static void
pas_backend_summary_dispose (GObject *object)
{
	PASBackendSummary *summary = PAS_BACKEND_SUMMARY (object);

	if (summary->priv) {
		if (summary->priv->dirty)
			g_warning ("Destroying dirty summary");

		if (summary->priv->flush_timeout) {
			g_source_remove (summary->priv->flush_timeout);
			summary->priv->flush_timeout = 0;
		}

		if (summary->priv->fp)
			fclose (summary->priv->fp);

		g_free (summary->priv->summary_path);
		clear_items (summary);
		g_ptr_array_free (summary->priv->items, TRUE);

		g_hash_table_destroy (summary->priv->id_to_item);

		g_free (summary->priv);
		summary->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
pas_backend_summary_class_init (PASBackendSummaryClass *klass)
{
	GObjectClass  *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* Set the virtual methods. */

	object_class->dispose = pas_backend_summary_dispose;
}

static void
pas_backend_summary_init (PASBackendSummary *summary)
{
	PASBackendSummaryPrivate *priv;

	priv             = g_new(PASBackendSummaryPrivate, 1);

	summary->priv = priv;

	priv->summary_path = NULL;
	priv->fp = NULL;
	priv->dirty = FALSE;
	priv->upgraded = FALSE;
	priv->items = g_ptr_array_new();
	priv->id_to_item = g_hash_table_new (g_str_hash, g_str_equal);
	priv->flush_timeout_millis = 0;
	priv->flush_timeout = 0;
#ifdef SUMMARY_STATS
	priv->size = 0;
#endif
}

/**
 * pas_backend_summary_get_type:
 */
GType
pas_backend_summary_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (PASBackendSummaryClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  pas_backend_summary_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PASBackendSummary),
			0,    /* n_preallocs */
			(GInstanceInitFunc) pas_backend_summary_init
		};

		type = g_type_register_static (G_TYPE_OBJECT, "PASBackendSummary", &info, 0);
	}

	return type;
}


static gboolean
pas_backend_summary_check_magic (PASBackendSummary *summary, FILE *fp)
{
	char buf [PAS_SUMMARY_MAGIC_LEN + 1];
	int rv;

	memset (buf, 0, sizeof (buf));

	rv = fread (buf, PAS_SUMMARY_MAGIC_LEN, 1, fp);
	if (rv != 1)
		return FALSE;
	if (strcmp (buf, PAS_SUMMARY_MAGIC))
		return FALSE;

	return TRUE;
}

static gboolean
pas_backend_summary_load_header (PASBackendSummary *summary, FILE *fp,
				 PASBackendSummaryHeader *header)
{
	int rv;

	rv = fread (&header->file_version, sizeof (header->file_version), 1, fp);
	if (rv != 1)
		return FALSE;

	header->file_version = ntohl (header->file_version);

	if (header->file_version < PAS_SUMMARY_FILE_VERSION) {
		return FALSE; /* this will cause the entire summary to be rebuilt */
	}

	rv = fread (&header->num_items, sizeof (header->num_items), 1, fp);
	if (rv != 1)
		return FALSE;

	header->num_items = ntohl (header->num_items);

	rv = fread (&header->summary_mtime, sizeof (header->summary_mtime), 1, fp);
	if (rv != 1)
		return FALSE;
	header->summary_mtime = ntohl (header->summary_mtime);

	return TRUE;
}

static char *
read_string (FILE *fp, int len)
{
	char *buf;
	int rv;

	buf = g_new0 (char, len + 1);

	rv = fread (buf, len, 1, fp);
	if (rv != 1) {
		g_free (buf);
		return NULL;
	}

	return buf;
}

static gboolean
pas_backend_summary_load_item (PASBackendSummary *summary,
			       PASBackendSummaryItem **new_item)
{
	PASBackendSummaryItem *item;
	char *buf;
	FILE *fp = summary->priv->fp;

	if (summary->priv->file_version >= PAS_SUMMARY_FILE_VERSION_4_0) {
		PASBackendSummaryDiskItem disk_item;
		int rv = fread (&disk_item, sizeof (disk_item), 1, fp);
		if (rv != 1)
			return FALSE;

		disk_item.id_len = ntohs (disk_item.id_len);
		disk_item.nickname_len = ntohs (disk_item.nickname_len);
		disk_item.full_name_len = ntohs (disk_item.full_name_len);
		disk_item.given_name_len = ntohs (disk_item.given_name_len);
		disk_item.surname_len = ntohs (disk_item.surname_len);
		disk_item.file_as_len = ntohs (disk_item.file_as_len);
		disk_item.email_1_len = ntohs (disk_item.email_1_len);
		disk_item.email_2_len = ntohs (disk_item.email_2_len);
		disk_item.email_3_len = ntohs (disk_item.email_3_len);

		item = g_new0 (PASBackendSummaryItem, 1);

		item->wants_html = disk_item.wants_html;
		item->wants_html_set = disk_item.wants_html_set;
		item->list = disk_item.list;
		item->list_show_addresses = disk_item.list_show_addresses;

		if (disk_item.id_len) {
			buf = read_string (fp, disk_item.id_len);
			if (!buf) {
				free_summary_item (item);
				return FALSE;
			}
			item->id = buf;
		}

		if (disk_item.nickname_len) {
			buf = read_string (fp, disk_item.nickname_len);
			if (!buf) {
				free_summary_item (item);
				return FALSE;
			}
			item->nickname = buf;
		}

		if (disk_item.full_name_len) {
			buf = read_string (fp, disk_item.full_name_len);
			if (!buf) {
				free_summary_item (item);
				return FALSE;
			}
			item->full_name = buf;
		}

		if (disk_item.given_name_len) {
			buf = read_string (fp, disk_item.given_name_len);
			if (!buf) {
				free_summary_item (item);
				return FALSE;
			}
			item->given_name = buf;
		}

		if (disk_item.surname_len) {
			buf = read_string (fp, disk_item.surname_len);
			if (!buf) {
				free_summary_item (item);
				return FALSE;
			}
			item->surname = buf;
		}

		if (disk_item.file_as_len) {
			buf = read_string (fp, disk_item.file_as_len);
			if (!buf) {
				free_summary_item (item);
				return FALSE;
			}
			item->file_as = buf;
		}

		if (disk_item.email_1_len) {
			buf = read_string (fp, disk_item.email_1_len);
			if (!buf) {
				free_summary_item (item);
				return FALSE;
			}
			item->email_1 = buf;
		}

		if (disk_item.email_2_len) {
			buf = read_string (fp, disk_item.email_2_len);
			if (!buf) {
				free_summary_item (item);
				return FALSE;
			}
			item->email_2 = buf;
		}

		if (disk_item.email_3_len) {
			buf = read_string (fp, disk_item.email_3_len);
			if (!buf) {
				free_summary_item (item);
				return FALSE;
			}
			item->email_3 = buf;
		}

		/* the only field that has to be there is the id */
		if (!item->id) {
			free_summary_item (item);
			return FALSE;
		}
	}
	else {
		/* unhandled file version */
		return FALSE;
	}

	*new_item = item;
	return TRUE;
}

/* opens the file and loads the header */
static gboolean
pas_backend_summary_open (PASBackendSummary *summary)
{
	FILE *fp;
	PASBackendSummaryHeader header;
	struct stat sb;

	if (summary->priv->fp)
		return TRUE;

	if (stat (summary->priv->summary_path, &sb) == -1) {
		/* if there's no summary present, look for the .new
		   file and rename it if it's there, and attempt to
		   load that */
		char *new_filename = g_strconcat (summary->priv->summary_path, ".new", NULL);
		if (stat (new_filename, &sb) == -1) {
			g_warning ("no summary present");
			g_free (new_filename);
			return FALSE;
		}
		else {
			rename (new_filename, summary->priv->summary_path);
			g_free (new_filename);
		}
	}

	fp = fopen (summary->priv->summary_path, "r");
	if (!fp) {
		g_warning ("failed to open summary file");
		return FALSE;
	}

	if (!pas_backend_summary_check_magic (summary, fp)) {
		g_warning ("file is not a valid summary file");
		fclose (fp);
		return FALSE;
	}

	if (!pas_backend_summary_load_header (summary, fp, &header)) {
		g_warning ("failed to read summary header");
		fclose (fp);
		return FALSE;
	}

	summary->priv->num_items = header.num_items;
	summary->priv->file_version = header.file_version;
	summary->priv->mtime = header.summary_mtime;
	summary->priv->fp = fp;

	return TRUE;
}

gboolean
pas_backend_summary_load (PASBackendSummary *summary)
{
	PASBackendSummaryItem *new_item;
	int i;
	
	if (!pas_backend_summary_open (summary))
		return FALSE;

	for (i = 0; i < summary->priv->num_items; i ++) {
		if (!pas_backend_summary_load_item (summary, &new_item)) {
			g_warning ("error while reading summary item");
			clear_items (summary);
			fclose (summary->priv->fp);
			summary->priv->fp = NULL;
			summary->priv->dirty = FALSE;
			return FALSE;
		}

		g_ptr_array_add (summary->priv->items, new_item);
		g_hash_table_insert (summary->priv->id_to_item, new_item->id, new_item);
	}

	if (summary->priv->upgraded) {
		pas_backend_summary_save (summary);
	}
	summary->priv->dirty = FALSE;

	return TRUE;
}

static gboolean
pas_backend_summary_save_magic (FILE *fp)
{
	int rv;
	rv = fwrite (PAS_SUMMARY_MAGIC, PAS_SUMMARY_MAGIC_LEN, 1, fp);
	if (rv != 1)
		return FALSE;

	return TRUE;
}

static gboolean
pas_backend_summary_save_header (PASBackendSummary *summary, FILE *fp)
{
	PASBackendSummaryHeader header;
	int rv;

	header.file_version = htonl (PAS_SUMMARY_FILE_VERSION);
	header.num_items = htonl (summary->priv->items->len);
	header.summary_mtime = htonl (time (NULL));

	rv = fwrite (&header, sizeof (header), 1, fp);
	if (rv != 1)
		return FALSE;

	return TRUE;
}

static gboolean
save_string (const char *str, FILE *fp)
{
	int rv;

	if (!str || !*str)
		return TRUE;

	rv = fwrite (str, strlen (str), 1, fp);
	return (rv == 1);
}

static gboolean
pas_backend_summary_save_item (PASBackendSummary *summary, FILE *fp, PASBackendSummaryItem *item)
{
	PASBackendSummaryDiskItem disk_item;
	int len;
	int rv;

	len = item->id ? strlen (item->id) : 0;
	disk_item.id_len = htons (len);

	len = item->nickname ? strlen (item->nickname) : 0;
	disk_item.nickname_len = htons (len);

	len = item->given_name ? strlen (item->given_name) : 0;
	disk_item.given_name_len = htons (len);

	len = item->full_name ? strlen (item->full_name) : 0;
	disk_item.full_name_len = htons (len);

	len = item->surname ? strlen (item->surname) : 0;
	disk_item.surname_len = htons (len);

	len = item->file_as ? strlen (item->file_as) : 0;
	disk_item.file_as_len = htons (len);

	len = item->email_1 ? strlen (item->email_1) : 0;
	disk_item.email_1_len = htons (len);

	len = item->email_2 ? strlen (item->email_2) : 0;
	disk_item.email_2_len = htons (len);

	len = item->email_3 ? strlen (item->email_3) : 0;
	disk_item.email_3_len = htons (len);

	disk_item.wants_html = item->wants_html;
	disk_item.wants_html_set = item->wants_html_set;
	disk_item.list = item->list;
	disk_item.list_show_addresses = item->list_show_addresses;

	rv = fwrite (&disk_item, sizeof(disk_item), 1, fp);
	if (rv != 1)
		return FALSE;

	if (!save_string (item->id, fp))
		return FALSE;
	if (!save_string (item->nickname, fp))
		return FALSE;
	if (!save_string (item->full_name, fp))
		return FALSE;
	if (!save_string (item->given_name, fp))
		return FALSE;
	if (!save_string (item->surname, fp))
		return FALSE;
	if (!save_string (item->file_as, fp))
		return FALSE;
	if (!save_string (item->email_1, fp))
		return FALSE;
	if (!save_string (item->email_2, fp))
		return FALSE;
	if (!save_string (item->email_3, fp))
		return FALSE;

	return TRUE;
}

gboolean
pas_backend_summary_save (PASBackendSummary *summary)
{
	struct stat sb;
	FILE *fp = NULL;
	char *new_filename = NULL;
	int i;

	if (!summary->priv->dirty)
		return TRUE;

	new_filename = g_strconcat (summary->priv->summary_path, ".new", NULL);

	fp = fopen (new_filename, "w");
	if (!fp) {
		g_warning ("could not create new summary file");
		goto lose;
	}

	if (!pas_backend_summary_save_magic (fp)) {
		g_warning ("could not write magic to new summary file");
		goto lose;
	}

	if (!pas_backend_summary_save_header (summary, fp)) {
		g_warning ("could not write header to new summary file");
		goto lose;
	}

	for (i = 0; i < summary->priv->items->len; i ++) {
		PASBackendSummaryItem *item = g_ptr_array_index (summary->priv->items, i);
		if (!pas_backend_summary_save_item (summary, fp, item)) {
			g_warning ("failed to write an item to new summary file, errno = %d", errno);
			goto lose;
		}
	}

	fclose (fp);

	/* if we have a queued flush, clear it (since we just flushed) */
	if (summary->priv->flush_timeout) {
		g_source_remove (summary->priv->flush_timeout);
		summary->priv->flush_timeout = 0;
	}

	/* unlink the old summary and rename the new one */
	unlink (summary->priv->summary_path);
	rename (new_filename, summary->priv->summary_path);

	g_free (new_filename);

	/* lastly, update the in memory mtime to that of the file */
	if (stat (summary->priv->summary_path, &sb) == -1) {
		g_warning ("error stat'ing saved summary");
	}
	else {
		summary->priv->mtime = sb.st_mtime;
	}

	return TRUE;

 lose:
	if (fp)
		fclose (fp);
	if (new_filename)
		unlink (new_filename);
	g_free (new_filename);
	return FALSE;
}

void
pas_backend_summary_add_contact (PASBackendSummary *summary, EContact *contact)
{
	PASBackendSummaryItem *new_item;

	new_item = g_new0 (PASBackendSummaryItem, 1);

	new_item->id         = e_contact_get (contact, E_CONTACT_UID);
	new_item->nickname   = e_contact_get (contact, E_CONTACT_NICKNAME);
	new_item->full_name  = e_contact_get (contact, E_CONTACT_FULL_NAME);
	new_item->given_name = e_contact_get (contact, E_CONTACT_GIVEN_NAME);
	new_item->surname    = e_contact_get (contact, E_CONTACT_FAMILY_NAME);
	new_item->file_as    = e_contact_get (contact, E_CONTACT_FILE_AS);
	new_item->email_1    = e_contact_get (contact, E_CONTACT_EMAIL_1);
	new_item->email_2    = e_contact_get (contact, E_CONTACT_EMAIL_2);
	new_item->email_3    = e_contact_get (contact, E_CONTACT_EMAIL_3);
	new_item->list       = GPOINTER_TO_INT (e_contact_get (contact, E_CONTACT_IS_LIST));
	new_item->list_show_addresses = GPOINTER_TO_INT (e_contact_get (contact, E_CONTACT_LIST_SHOW_ADDRESSES));
	new_item->wants_html = GPOINTER_TO_INT (e_contact_get (contact, E_CONTACT_WANTS_HTML));

	g_ptr_array_add (summary->priv->items, new_item);
	g_hash_table_insert (summary->priv->id_to_item, new_item->id, new_item);

	g_object_unref (contact);

#ifdef SUMMARY_STATS
	summary->priv->size += sizeof (PASBackendSummaryItem);
	summary->priv->size += new_item->id ? strlen (new_item->id) : 0;
	summary->priv->size += new_item->nickname ? strlen (new_item->nickname) : 0;
	summary->priv->size += new_item->full_name ? strlen (new_item->full_name) : 0;
	summary->priv->size += new_item->given_name ? strlen (new_item->given_name) : 0;
	summary->priv->size += new_item->surname ? strlen (new_item->surname) : 0;
	summary->priv->size += new_item->file_as ? strlen (new_item->file_as) : 0;
	summary->priv->size += new_item->email_1 ? strlen (new_item->email_1) : 0;
	summary->priv->size += new_item->email_2 ? strlen (new_item->email_2) : 0;
	summary->priv->size += new_item->email_3 ? strlen (new_item->email_3) : 0;
#endif
	pas_backend_summary_touch (summary);
}

void
pas_backend_summary_remove_contact (PASBackendSummary *summary, const char *id)
{
	PASBackendSummaryItem *item = g_hash_table_lookup (summary->priv->id_to_item, id);

	if (item) {
		g_ptr_array_remove (summary->priv->items, item);
		g_hash_table_remove (summary->priv->id_to_item, id);
		free_summary_item (item);
		pas_backend_summary_touch (summary);
		return;
	}

	g_warning ("pas_backend_summary_remove_contact: unable to locate id `%s'", id);
}

static gboolean
summary_flush_func (gpointer data)
{
	PASBackendSummary *summary = PAS_BACKEND_SUMMARY (data);

	if (!summary->priv->dirty) {
		summary->priv->flush_timeout = 0;
		return FALSE;
	}

	if (!pas_backend_summary_save (summary)) {
		/* this isn't fatal, as we can just either 1) flush
		   out with the next change, or 2) regen the summary
		   when we next load the uri */
		g_warning ("failed to flush summary file to disk");
		return TRUE; /* try again after the next timeout */
	}

	g_warning ("flushed summary to disk");

	/* we only want this to execute once, so return FALSE and set
	   summary->flush_timeout to 0 */
	summary->priv->flush_timeout = 0;
	return FALSE;
}

void
pas_backend_summary_touch (PASBackendSummary *summary)
{
	summary->priv->dirty = TRUE;
	if (!summary->priv->flush_timeout
	    && summary->priv->flush_timeout_millis)
		summary->priv->flush_timeout = g_timeout_add (summary->priv->flush_timeout_millis,
							      summary_flush_func, summary);
}

gboolean
pas_backend_summary_is_up_to_date (PASBackendSummary *summary, time_t t)
{
	if (!pas_backend_summary_open (summary))
		return FALSE;
	else
		return summary->priv->mtime >= t;
}


/* we only want to do summary queries if the query is over the set fields in the summary */

static ESExpResult *
func_check(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	ESExpResult *r;
	int truth = FALSE;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *query_name = argv[0]->value.string;

		if (!strcmp (query_name, "nickname") ||
		    !strcmp (query_name, "full_name") ||
		    !strcmp (query_name, "file_as") ||
		    !strcmp (query_name, "email")) {
			truth = TRUE;
		}
	}

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = truth;
	
	return r;
}

/* 'builtin' functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} check_symbols[] = {
	{ "contains", func_check, 0 },
	{ "is", func_check, 0 },
	{ "beginswith", func_check, 0 },
	{ "endswith", func_check, 0 },
};

gboolean
pas_backend_summary_is_summary_query (PASBackendSummary *summary, const char *query)
{
	ESExp *sexp;
	ESExpResult *r;
	gboolean retval;
	int i;
	int esexp_error;

	sexp = e_sexp_new();

	for(i=0;i<sizeof(check_symbols)/sizeof(check_symbols[0]);i++) {
		if (check_symbols[i].type == 1) {
			e_sexp_add_ifunction(sexp, 0, check_symbols[i].name,
					     (ESExpIFunc *)check_symbols[i].func, summary);
		} else {
			e_sexp_add_function(sexp, 0, check_symbols[i].name,
					    check_symbols[i].func, summary);
		}
	}

	e_sexp_input_text(sexp, query, strlen(query));
	esexp_error = e_sexp_parse(sexp);

	if (esexp_error == -1) {
		return FALSE;
	}

	r = e_sexp_eval(sexp);

	retval = (r && r->type == ESEXP_RES_BOOL && r->value.bool);

	e_sexp_result_free(sexp, r);

	e_sexp_unref (sexp);

	return retval;
}



/* the actual query mechanics */
static ESExpResult *
do_compare (PASBackendSummary *summary, struct _ESExp *f, int argc,
	    struct _ESExpResult **argv,
	    char *(*compare)(const char*, const char*))
{
	GPtrArray *result = g_ptr_array_new ();
	ESExpResult *r;
	int i;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {

		for (i = 0; i < summary->priv->items->len; i ++) {
			PASBackendSummaryItem *item = g_ptr_array_index (summary->priv->items, i);
			if (!strcmp (argv[0]->value.string, "full_name")) {
				char *given = item->given_name;
				char *surname = item->surname;
				if ((given && compare (given, argv[1]->value.string))
				    || (surname && compare (surname, argv[1]->value.string)))
					g_ptr_array_add (result, item->id);
			}
			else if (!strcmp (argv[0]->value.string, "email")) {
				char *email_1 = item->email_1;
				char *email_2 = item->email_2;
				char *email_3 = item->email_3;
				if ((email_1 && compare (email_1, argv[1]->value.string))
				    || (email_2 && compare (email_2, argv[1]->value.string))
				    || (email_3 && compare (email_3, argv[1]->value.string)))
					g_ptr_array_add (result, item->id);
			}
			else if (!strcmp (argv[0]->value.string, "file_as")) {
				char *file_as = item->file_as;
				if (file_as && compare (file_as, argv[1]->value.string))
					g_ptr_array_add (result, item->id);
			}
			else if (!strcmp (argv[0]->value.string, "nickname")) {
				char *nickname = item->nickname;
				if (nickname && compare (nickname, argv[1]->value.string))
					g_ptr_array_add (result, item->id);
			}
		}
	}

	r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
	r->value.ptrarray = result;

	return r;
}

static ESExpResult *
func_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	PASBackendSummary *summary = data;

	return do_compare (summary, f, argc, argv, (char *(*)(const char*, const char*)) e_utf8_strstrcase);
}

static char *
is_helper (const char *s1, const char *s2)
{
	if (!strcasecmp(s1, s2))
		return (char*)s1;
	else
		return NULL;
}

static ESExpResult *
func_is(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	PASBackendSummary *summary = data;

	return do_compare (summary, f, argc, argv, is_helper);
}

static char *
endswith_helper (const char *s1, const char *s2)
{
	char *p;
	if ((p = (char*)e_utf8_strstrcase(s1, s2))
	    && (strlen(p) == strlen(s2)))
		return p;
	else
		return NULL;
}

static ESExpResult *
func_endswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	PASBackendSummary *summary = data;

	return do_compare (summary, f, argc, argv, endswith_helper);
}

static char *
beginswith_helper (const char *s1, const char *s2)
{
	char *p;
	if ((p = (char*)e_utf8_strstrcase(s1, s2))
	    && (p == s1))
		return p;
	else
		return NULL;
}

static ESExpResult *
func_beginswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	PASBackendSummary *summary = data;

	return do_compare (summary, f, argc, argv, beginswith_helper);
}

/* 'builtin' functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "contains", func_contains, 0 },
	{ "is", func_is, 0 },
	{ "beginswith", func_beginswith, 0 },
	{ "endswith", func_endswith, 0 },
};

GPtrArray*
pas_backend_summary_search (PASBackendSummary *summary, const char *query)
{
	ESExp *sexp;
	ESExpResult *r;
	GPtrArray *retval = g_ptr_array_new();
	int i;
	int esexp_error;

	sexp = e_sexp_new();

	for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction(sexp, 0, symbols[i].name,
					     (ESExpIFunc *)symbols[i].func, summary);
		} else {
			e_sexp_add_function(sexp, 0, symbols[i].name,
					    symbols[i].func, summary);
		}
	}

	e_sexp_input_text(sexp, query, strlen(query));
	esexp_error = e_sexp_parse(sexp);

	if (esexp_error == -1) {
		return NULL;
	}

	r = e_sexp_eval(sexp);

	if (r && r->type == ESEXP_RES_ARRAY_PTR && r->value.ptrarray) {
		GPtrArray *ptrarray = r->value.ptrarray;
		int i;

		for (i = 0; i < ptrarray->len; i ++)
			g_ptr_array_add (retval, g_ptr_array_index (ptrarray, i));
	}

	e_sexp_result_free(sexp, r);

	e_sexp_unref (sexp);

	return retval;
}

char*
pas_backend_summary_get_summary_vcard(PASBackendSummary *summary, const char *id)
{
	PASBackendSummaryItem *item = g_hash_table_lookup (summary->priv->id_to_item, id);

	if (item) {
		EContact *contact = e_contact_new ();
		char *vcard;

		e_contact_set (contact, E_CONTACT_UID, item->id);
		e_contact_set (contact, E_CONTACT_FILE_AS, item->file_as);
		e_contact_set (contact, E_CONTACT_GIVEN_NAME, item->given_name);
		e_contact_set (contact, E_CONTACT_FAMILY_NAME, item->surname);
		e_contact_set (contact, E_CONTACT_NICKNAME, item->nickname);
		e_contact_set (contact, E_CONTACT_FULL_NAME, item->full_name);
		e_contact_set (contact, E_CONTACT_EMAIL_1, item->email_1);
		e_contact_set (contact, E_CONTACT_EMAIL_2, item->email_2);
		e_contact_set (contact, E_CONTACT_EMAIL_3, item->email_3);

		e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (item->list));
		e_contact_set (contact, E_CONTACT_LIST_SHOW_ADDRESSES, GINT_TO_POINTER (item->list_show_addresses));
		e_contact_set (contact, E_CONTACT_WANTS_HTML, GINT_TO_POINTER (item->wants_html));

		vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

		g_object_unref (contact);

		return vcard;
	}
	else {
		g_warning ("in unable to locate card `%s' in summary", id);
		return NULL;
	}
}

