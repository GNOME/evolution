/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright 2000-2003 Ximian, Inc. (www.ximian.com)
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

#include <glib.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "camel-nntp-newsrc.h"
#include <camel/camel-folder-summary.h>

#define NEWSRC_LOCK(f, l) (g_mutex_lock(((CamelNNTPNewsrc *)f)->l))
#define NEWSRC_UNLOCK(f, l) (g_mutex_unlock(((CamelNNTPNewsrc *)f)->l))

typedef struct {
	guint low;
	guint high;
} ArticleRange;

typedef struct {
	char *name;
	GArray *ranges;
	gboolean subscribed;
} NewsrcGroup;

struct CamelNNTPNewsrc {
	gchar *filename;
	GHashTable *groups;
	gboolean dirty;
	GMutex *lock;
};


static NewsrcGroup *
camel_nntp_newsrc_group_add (CamelNNTPNewsrc *newsrc, const char *group_name, gboolean subscribed)
{
	NewsrcGroup *new_group = g_malloc(sizeof(NewsrcGroup));

	new_group->name = g_strdup(group_name);
	new_group->subscribed = subscribed;
	new_group->ranges = g_array_new (FALSE, FALSE, sizeof (ArticleRange));

	g_hash_table_insert (newsrc->groups, new_group->name, new_group);

	newsrc->dirty = TRUE;

	return new_group;
}

static int
camel_nntp_newsrc_group_get_highest_article_read(CamelNNTPNewsrc *newsrc, NewsrcGroup *group)
{
	if (!group || group->ranges->len == 0)
		return 0;

	return g_array_index(group->ranges, ArticleRange, group->ranges->len - 1).high;
}

static int
camel_nntp_newsrc_group_get_num_articles_read(CamelNNTPNewsrc *newsrc, NewsrcGroup *group)
{
	int i;
	int count = 0;

	if (group == NULL)
		return 0;

	for (i = 0; i < group->ranges->len; i ++)
		count += (g_array_index(group->ranges, ArticleRange, i).high -
			  g_array_index(group->ranges, ArticleRange, i).low) + 1;

	return count;
}


static void
camel_nntp_newsrc_group_mark_range_read(CamelNNTPNewsrc *newsrc, NewsrcGroup *group, long low, long high)
{
	int i;

	if (group->ranges->len == 1
	    && g_array_index (group->ranges, ArticleRange, 0).low == 0
	    && g_array_index (group->ranges, ArticleRange, 0).high == 0) {
		g_array_index (group->ranges, ArticleRange, 0).low = low;
		g_array_index (group->ranges, ArticleRange, 0).high = high;

		newsrc->dirty = TRUE;
	}
	else  {
		ArticleRange tmp_range;

		for (i = 0; i < group->ranges->len; i ++) {
			guint range_low = g_array_index (group->ranges, ArticleRange, i).low;
			guint range_high = g_array_index (group->ranges, ArticleRange, i).high;
			
			/* if it's already part of a range, return immediately. */
			if (low >= range_low &&
			    low <= range_high &&
			    high >= range_low &&
			    high <= range_high) {
				return;
			}
			/* if we have a new lower bound for this range, set it. */
			else if (low <= range_low
				 && high >= range_low
				 && high <= range_high) {
				g_array_index (group->ranges, ArticleRange, i).low = low;
				newsrc->dirty = TRUE;
				return;
			}
			/* if we have a new upper bound for this range, set it. */
			else if (high >= range_high
				 && low >= range_low
				 && low <= range_high) {
				g_array_index (group->ranges, ArticleRange, i).high = high;
				newsrc->dirty = TRUE;
				return;
			}
			/* if we would be inserting another range that
                           starts one index higher than an existing
                           one, make the upper value of the existing
                           range the upper value of the new one. */
			else if (low == range_high + 1) {
				g_array_index (group->ranges, ArticleRange, i).high = high;
				newsrc->dirty = TRUE;
				return;
			}
			/* if we would be inserting another range that
                           ends one index lower than an existing one,
                           group the existing range by setting its low
                           to the new low */
			else if (high == range_low - 1) {
				g_array_index (group->ranges, ArticleRange, i).low = low;
				newsrc->dirty = TRUE;
				return;
			}
			/* if the range lies entirely outside another
                           range, doesn't coincide with it's
                           endpoints, and has lower values, insert it
                           into the middle of the list. */
			else if (low < range_low
				 && high < range_low) {
				tmp_range.low = low;
				tmp_range.high = high;

				group->ranges = g_array_insert_val (group->ranges, i, tmp_range);
				newsrc->dirty = TRUE;

				return;
			}
		}

		/* if we made it here, the range needs to go at the end */
		tmp_range.low = low;
		tmp_range.high = high;
		group->ranges = g_array_append_val (group->ranges, tmp_range);
		newsrc->dirty = TRUE;
	} 
}

int
camel_nntp_newsrc_get_highest_article_read (CamelNNTPNewsrc *newsrc, const char *group_name)
{
	NewsrcGroup *group;
	int ret;

	NEWSRC_LOCK(newsrc, lock);

	group = g_hash_table_lookup (newsrc->groups, group_name);

	if (group)
		ret = camel_nntp_newsrc_group_get_highest_article_read (newsrc, group);
	else
		ret = 0;

	NEWSRC_UNLOCK(newsrc, lock);

	return ret;
}

int
camel_nntp_newsrc_get_num_articles_read (CamelNNTPNewsrc *newsrc, const char *group_name)
{
	NewsrcGroup *group;
	int ret;

	NEWSRC_LOCK(newsrc, lock);

	group = g_hash_table_lookup (newsrc->groups, group_name);

	if (group)
		ret = camel_nntp_newsrc_group_get_num_articles_read (newsrc, group);
	else
		ret = 0;

	NEWSRC_UNLOCK(newsrc, lock);

	return ret;
}

void
camel_nntp_newsrc_mark_article_read (CamelNNTPNewsrc *newsrc, const char *group_name, int num)
{
	camel_nntp_newsrc_mark_range_read (newsrc, group_name, num, num);
}

void
camel_nntp_newsrc_mark_range_read(CamelNNTPNewsrc *newsrc, const char *group_name, long low, long high)
{
	NewsrcGroup *group;

	/* swap them if they're in the wrong order. */
	if (low > high) {
		long tmp;

		tmp = high;
		high = low;
		low = tmp;
	}

	NEWSRC_LOCK(newsrc, lock);
	group = g_hash_table_lookup (newsrc->groups, group_name);

	if (group)
		camel_nntp_newsrc_group_mark_range_read (newsrc, group, low, high);
	NEWSRC_UNLOCK(newsrc, lock);
}

gboolean
camel_nntp_newsrc_article_is_read (CamelNNTPNewsrc *newsrc, const char *group_name, long num)
{
	int i;
	NewsrcGroup *group;
	int ret = FALSE;

	NEWSRC_LOCK(newsrc, lock);
	group = g_hash_table_lookup (newsrc->groups, group_name);

	if (group) {
		for (i = 0; i < group->ranges->len; i++) {
			if (num >= g_array_index (group->ranges, ArticleRange, i).low && 
			    num <= g_array_index (group->ranges, ArticleRange, i).high) {
				ret = TRUE;
				break;
			}
		}
	}

	NEWSRC_UNLOCK(newsrc, lock);

	return FALSE;
}

gboolean  
camel_nntp_newsrc_group_is_subscribed (CamelNNTPNewsrc *newsrc, const char *group_name)
{
	NewsrcGroup *group;
	int ret = FALSE;

	NEWSRC_LOCK(newsrc, lock);

	group = g_hash_table_lookup (newsrc->groups, group_name);

	if (group) {
		ret = group->subscribed;
	}

	NEWSRC_UNLOCK(newsrc, lock);

	return ret;
}

void
camel_nntp_newsrc_subscribe_group (CamelNNTPNewsrc *newsrc, const char *group_name)
{
	NewsrcGroup *group;

	NEWSRC_LOCK(newsrc, lock);

	group = g_hash_table_lookup (newsrc->groups, group_name);

	if (group) {
		if (!group->subscribed)
			newsrc->dirty = TRUE;
		group->subscribed = TRUE;
	}
	else {
		camel_nntp_newsrc_group_add (newsrc, group_name, TRUE);
	}

	NEWSRC_UNLOCK(newsrc, lock);
}

void
camel_nntp_newsrc_unsubscribe_group (CamelNNTPNewsrc *newsrc, const char *group_name)
{
	NewsrcGroup *group;

	NEWSRC_LOCK(newsrc, lock);

	group = g_hash_table_lookup (newsrc->groups, group_name);
	if (group) {
		if (group->subscribed)
			newsrc->dirty = TRUE;
		group->subscribed = FALSE;
	}
	else {
		camel_nntp_newsrc_group_add (newsrc, group_name, FALSE);
	}

	NEWSRC_UNLOCK(newsrc, lock);
}

struct newsrc_ptr_array {
	GPtrArray *ptr_array;
	gboolean subscribed_only;
};

/* this needs to strdup the grup_name, if the group array is likely to change */
static void
get_group_foreach (char *group_name, NewsrcGroup *group, struct newsrc_ptr_array *npa)
{
	if (group->subscribed || !npa->subscribed_only) {
		g_ptr_array_add (npa->ptr_array, group_name);
	}
}

GPtrArray *
camel_nntp_newsrc_get_subscribed_group_names (CamelNNTPNewsrc *newsrc)
{
	struct newsrc_ptr_array npa;

	g_return_val_if_fail (newsrc, NULL);

	NEWSRC_LOCK(newsrc, lock);

	npa.ptr_array = g_ptr_array_new();
	npa.subscribed_only = TRUE;

	g_hash_table_foreach (newsrc->groups,
			      (GHFunc)get_group_foreach, &npa);

	NEWSRC_UNLOCK(newsrc, lock);

	return npa.ptr_array;
}

GPtrArray *
camel_nntp_newsrc_get_all_group_names (CamelNNTPNewsrc *newsrc)
{
	struct newsrc_ptr_array npa;

	g_return_val_if_fail (newsrc, NULL);

	NEWSRC_LOCK(newsrc, lock);

	npa.ptr_array = g_ptr_array_new();
	npa.subscribed_only = FALSE;

	g_hash_table_foreach (newsrc->groups,
			      (GHFunc)get_group_foreach, &npa);

	NEWSRC_UNLOCK(newsrc, lock);

	return npa.ptr_array;
}

void
camel_nntp_newsrc_free_group_names (CamelNNTPNewsrc *newsrc, GPtrArray *group_names)
{
	g_ptr_array_free (group_names, TRUE);
}

struct newsrc_fp {
	CamelNNTPNewsrc *newsrc;
	FILE *fp;
};

static void
camel_nntp_newsrc_write_group_line(gpointer key, NewsrcGroup *group, struct newsrc_fp *newsrc_fp)
{
	CamelNNTPNewsrc *newsrc;
	FILE *fp;
	int i;

	fp = newsrc_fp->fp;
	newsrc = newsrc_fp->newsrc;

	fprintf (fp, "%s%c", group->name, group->subscribed ? ':' : '!');

	if (group->ranges->len == 1
	    && g_array_index (group->ranges, ArticleRange, 0).low == 0
	    && g_array_index (group->ranges, ArticleRange, 0).high == 0) {
		fprintf (fp, "\n");

		return; /* special case since our parsing code will insert this
			   bogus range if there were no read articles.  The code
			   to add a range is smart enough to remove this one if we
			   ever mark an article read, but we still need to deal with
			   it if that code doesn't get hit. */
	}

	fprintf (fp, " ");

	for (i = 0; i < group->ranges->len; i ++) {
		char range_buffer[100];
		guint low = g_array_index (group->ranges, ArticleRange, i).low;
		guint high = g_array_index (group->ranges, ArticleRange, i).high;

		if (low == high)
			sprintf(range_buffer, "%d", low);
		else if (low == high - 1)
			sprintf(range_buffer, "%d,%d", low, high);
		else
			sprintf(range_buffer, "%d-%d", low, high);

		if (i != group->ranges->len - 1)
			strcat(range_buffer, ",");

		fprintf (fp, range_buffer);
	}

	fprintf (fp, "\n");
}

void 
camel_nntp_newsrc_write_to_file(CamelNNTPNewsrc *newsrc, FILE *fp)
{
	struct newsrc_fp newsrc_fp;

	g_return_if_fail (newsrc);

	newsrc_fp.newsrc = newsrc;
	newsrc_fp.fp = fp;

	NEWSRC_LOCK(newsrc, lock);

	g_hash_table_foreach (newsrc->groups,
			      (GHFunc)camel_nntp_newsrc_write_group_line,
			      &newsrc_fp);

	NEWSRC_UNLOCK(newsrc, lock);
}

void
camel_nntp_newsrc_write(CamelNNTPNewsrc *newsrc)
{
	FILE *fp;

	g_return_if_fail (newsrc);

	NEWSRC_LOCK(newsrc, lock);

	if (!newsrc->dirty) {
		NEWSRC_UNLOCK(newsrc, lock);
		return;
	}

	if ((fp = fopen(newsrc->filename, "w")) == NULL) {
		g_warning ("Couldn't open newsrc file '%s'.\n", newsrc->filename);
		NEWSRC_UNLOCK(newsrc, lock);
		return;
	}

	newsrc->dirty = FALSE;
	NEWSRC_UNLOCK(newsrc, lock);

	camel_nntp_newsrc_write_to_file(newsrc, fp);

	fclose(fp);
}

static void
camel_nntp_newsrc_parse_line(CamelNNTPNewsrc *newsrc, char *line)
{
	char *p, *comma, *dash;
	gboolean is_subscribed;
	NewsrcGroup *group;

	p = strchr(line, ':');

	if (p) {
		is_subscribed = TRUE;
	}
	else {
		p = strchr(line, '!');
		if (p)
			is_subscribed = FALSE;
		else
			return; /* bogus line. */
	}

	*p++ = '\0';

	group = camel_nntp_newsrc_group_add (newsrc, line, is_subscribed);

	do {
		guint high, low;

		comma = strchr(p, ',');

		if (comma)
			*comma = '\0';

		dash = strchr(p, '-');

		if (!dash) { /* there wasn't a dash.  must be just one number */
			high = low = atol(p);
		}
		else { /* there was a dash. */
			*dash = '\0';
			low = atol(p);
			*dash = '-';
			p = dash + 1;
			high = atol(p);
		}

		camel_nntp_newsrc_group_mark_range_read (newsrc, group, low, high);

		if (comma) {
			*comma = ',';
			p = comma + 1;
		}

	} while(comma);
}

static char*
get_line (char *buf, char **p)
{
	char *l;
	char *line;

	g_assert (*p == NULL || **p == '\n' || **p == '\0');

	if (*p == NULL) {
		*p = buf;

		if (**p == '\0')
			return NULL;
	}
	else {
		if (**p == '\0')
			return NULL;

		(*p) ++;
	
		/* if we just incremented to the end of the buffer, return NULL */
		if (**p == '\0')
			return NULL;
	}

	l = strchr (*p, '\n');
	if (l) {
		*l = '\0';
		line = g_strdup (*p);
		*l = '\n';
		*p = l;
	}
	else {
		/* we're at the last line (which isn't terminated by a \n, btw) */
		line = g_strdup (*p);
		(*p) += strlen (*p);
	}

	return line;
}

CamelNNTPNewsrc *
camel_nntp_newsrc_read_for_server (const char *server)
{
	int fd;
	char buf[1024];
	char *file_contents, *line, *p;
	char *filename;
	CamelNNTPNewsrc *newsrc;
	int newsrc_len;
	int len_read = 0;
	struct stat sb;

	filename = g_strdup_printf ("%s/.newsrc-%s", g_get_home_dir(), server);

	newsrc = g_new0(CamelNNTPNewsrc, 1);
	newsrc->filename = filename;
	newsrc->groups = g_hash_table_new (g_str_hash, g_str_equal);
	newsrc->lock = g_mutex_new();
	
	if ((fd = open(filename, O_RDONLY)) == -1) {
		g_warning ("~/.newsrc-%s not present.\n", server);
		return newsrc;
	}

	if (fstat (fd, &sb) == -1) {
		g_warning ("failed fstat on ~/.newsrc-%s: %s\n", server, strerror(errno));
		return newsrc;
	}
	newsrc_len = sb.st_size;

	file_contents = g_malloc (newsrc_len + 1);

	while (len_read < newsrc_len) {
		int c = read (fd, buf, sizeof (buf));

		if (c == -1)
			break;

		memcpy (&file_contents[len_read], buf, c);
		len_read += c;
	}
	file_contents [len_read] = 0;

	p = NULL;
	while ((line = get_line (file_contents, &p))) {
		camel_nntp_newsrc_parse_line(newsrc, line);
		g_free (line);
	}

	close (fd);
	g_free (file_contents);

	return newsrc;
}
