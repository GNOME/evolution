/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-newsrc.c - .newsrc parsing/regurgitating code */
/* 
 *
 * Copyright (C) 2000 Helix Code, Inc. <toshok@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include "camel-nntp-newsrc.h"

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
	GHashTable *subscribed_groups;
	gboolean dirty;
} ;

static NewsrcGroup *
camel_nntp_newsrc_group_add (CamelNNTPNewsrc *newsrc, char *group_name, gboolean subscribed)
{
	NewsrcGroup *new_group = g_malloc(sizeof(NewsrcGroup));

	new_group->name = g_strdup(group_name);
	new_group->subscribed = subscribed;
	new_group->ranges = g_array_new (FALSE, FALSE, sizeof (ArticleRange));

	g_hash_table_insert (newsrc->groups, new_group->name, new_group);
	if (subscribed)
		g_hash_table_insert (newsrc->subscribed_groups, new_group->name, new_group);

	newsrc->dirty = TRUE;

	return new_group;
}

static long
camel_nntp_newsrc_group_get_highest_article_read(CamelNNTPNewsrc *newsrc, NewsrcGroup *group)
{
	if (group->ranges->len == 0)
		return 0;

	return g_array_index(group->ranges, ArticleRange, group->ranges->len - 1).high;
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
camel_nntp_newsrc_get_highest_article_read (CamelNNTPNewsrc *newsrc, char *group_name)
{
	NewsrcGroup *group;

	group = g_hash_table_lookup (newsrc->groups, group_name);

	return camel_nntp_newsrc_group_get_highest_article_read (newsrc, group);
}

void
camel_nntp_newsrc_mark_article_read (CamelNNTPNewsrc *newsrc, char *group_name, int num)
{
	camel_nntp_newsrc_mark_range_read (newsrc, group_name, num, num);
}

void
camel_nntp_newsrc_mark_range_read(CamelNNTPNewsrc *newsrc, char *group_name, long low, long high)
{
	NewsrcGroup *group;

	/* swap them if they're in the wrong order. */
	if (low > high) {
		long tmp;

		tmp = high;
		high = low;
		low = tmp;
	}

	group = g_hash_table_lookup (newsrc->groups, group_name);

	camel_nntp_newsrc_group_mark_range_read (newsrc, group, low, high);
}

gboolean
camel_nntp_newsrc_article_is_read (CamelNNTPNewsrc *newsrc, char *group_name, long num)
{
	int i;
	NewsrcGroup *group;

	group = g_hash_table_lookup (newsrc->groups, group_name);
	
	for (i = 0; i < group->ranges->len; i++) {
		if (num >= g_array_index (group->ranges, ArticleRange, i).low && 
		    num <= g_array_index (group->ranges, ArticleRange, i).high) {
			return TRUE;
		}
	}

	return FALSE;
}

struct newsrc_ptr_array {
	GPtrArray *ptr_array;
	gboolean subscribed_only;
};

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

	npa.ptr_array = g_ptr_array_new();
	npa.subscribed_only = TRUE;

	g_hash_table_foreach (newsrc->subscribed_groups,
			      (GHFunc)get_group_foreach, &npa);

	return npa.ptr_array;
}

GPtrArray *
camel_nntp_newsrc_get_all_group_names (CamelNNTPNewsrc *newsrc)
{
	struct newsrc_ptr_array npa;

	g_return_val_if_fail (newsrc, NULL);

	npa.ptr_array = g_ptr_array_new();
	npa.subscribed_only = FALSE;

	g_hash_table_foreach (newsrc->groups,
			      (GHFunc)get_group_foreach, &npa);

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
	int line_length = 0;

	fp = newsrc_fp->fp;
	newsrc = newsrc_fp->newsrc;

	fprintf (fp, "%s%c", group->name, group->subscribed ? ':' : '!');

	line_length += strlen(group->name) + 1;

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
	line_length += 1;

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

		/* this constant (991) gives the same line breaking as faried's .newsrc file */
		if (line_length + strlen(range_buffer) > 991 /*XXX*/) {
			char range_buffer2[101];
			int num_to_print = 991 - line_length;

			strcpy(range_buffer2, range_buffer);
			range_buffer2[num_to_print] = '!';
			range_buffer2[num_to_print+1] = '\n';
			range_buffer2[num_to_print+2] = '\0';

			fprintf (fp, range_buffer2);

			fprintf (fp, range_buffer + num_to_print);

			line_length = strlen(range_buffer) - num_to_print;
		}
		else {
			fprintf (fp, range_buffer);
			line_length += strlen(range_buffer);
		}
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

	g_hash_table_foreach (newsrc->groups,
			      (GHFunc)camel_nntp_newsrc_write_group_line,
			      &newsrc_fp);
}

void
camel_nntp_newsrc_write(CamelNNTPNewsrc *newsrc)
{
	FILE *fp;

	g_return_if_fail (newsrc);

	if (!newsrc->dirty)
		return;

	if ((fp = fopen(newsrc->filename, "w")) == NULL) {
		g_warning ("Couldn't open newsrc file '%s'.\n", newsrc->filename);
		return;
	}

	camel_nntp_newsrc_write_to_file(newsrc, fp);

	fclose(fp);
}

static void
camel_nntp_newsrc_parse_line(CamelNNTPNewsrc *newsrc, char *line)
{
	char *p, sep, *comma, *dash;
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

	sep = *p;
	*p = '\0';

	group = camel_nntp_newsrc_group_add (newsrc, line, is_subscribed);

	*p = sep;

	p++;

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

#define MAX_LINE_LENGTH 1500
#define BUFFER_LENGTH (20 * MAX_LINE_LENGTH)

CamelNNTPNewsrc *
camel_nntp_newsrc_read_for_server (const char *server)
{
	FILE *fp;
	char buf[BUFFER_LENGTH];
	CamelNNTPNewsrc *newsrc = g_new0(CamelNNTPNewsrc, 1);

	newsrc->filename = g_strdup_printf ("%s/.newsrc-%s", g_get_home_dir(), server);
	newsrc->groups = g_hash_table_new (g_str_hash, g_str_equal);
	newsrc->subscribed_groups = g_hash_table_new (g_str_hash, g_str_equal);

	if ((fp = fopen(newsrc->filename, "r")) == NULL) {
		g_free (newsrc->filename);
		g_free (newsrc);
		return NULL;
	}

	while (fgets(buf, MAX_LINE_LENGTH, fp) != NULL) {
		/* we silently ignore (and lose!) lines longer than 20 * 1500 chars.
		   Too bad for them.  */
		while(strlen(buf) < sizeof(buf) 
		      && buf[strlen(buf) - 2] == '!') {
			fgets(&buf[strlen(buf) - 2], MAX_LINE_LENGTH, fp);
		}

		camel_nntp_newsrc_parse_line(newsrc, buf);
	}

	fclose(fp);

	return newsrc;
}
