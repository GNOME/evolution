/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * pas-backend-summary.h
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

#ifndef __PAS_BACKEND_SUMMARY_H__
#define __PAS_BACKEND_SUMMARY_H__

#include <glib.h>
#include <glib-object.h>
#include <pas/pas-types.h>
#include <ebook/e-contact.h>

#define PAS_TYPE_BACKEND_SUMMARY        (pas_backend_summary_get_type ())
#define PAS_BACKEND_SUMMARY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BACKEND_SUMMARY, PASBackendSummary))
#define PAS_BACKEND_SUMMARY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_BACKEND_TYPE, PASBackendSummaryClass))
#define PAS_IS_BACKEND_SUMMARY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BACKEND_SUMMARY))
#define PAS_IS_BACKEND_SUMMARY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BACKEND_SUMMARY))
#define PAS_BACKEND_SUMMARY_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BACKEND_SUMMARY, PASBackendSummaryClass))

typedef struct _PASBackendSummaryPrivate PASBackendSummaryPrivate;

struct _PASBackendSummary{
	GObject parent_object;
	PASBackendSummaryPrivate *priv;
};

struct _PASBackendSummaryClass{
	GObjectClass parent_class;
};

PASBackendSummary* pas_backend_summary_new              (const char *summary_path,
							 int flush_timeout_millis);
GType              pas_backend_summary_get_type         (void);

/* returns FALSE if the load fails for any reason (including that the
   summary is out of date), TRUE if it succeeds */
gboolean           pas_backend_summary_load             (PASBackendSummary *summary);
/* returns FALSE if the save fails, TRUE if it succeeds (or isn't required due to no changes) */
gboolean           pas_backend_summary_save              (PASBackendSummary *summary);

void               pas_backend_summary_add_contact       (PASBackendSummary *summary, EContact *contact);
void               pas_backend_summary_remove_contact    (PASBackendSummary *summary, const char *id);

void               pas_backend_summary_touch             (PASBackendSummary *summary);

/* returns TRUE if the summary's mtime is >= @t. */
gboolean           pas_backend_summary_is_up_to_date     (PASBackendSummary *summary, time_t t);

gboolean           pas_backend_summary_is_summary_query  (PASBackendSummary *summary, const char *query);
GPtrArray*         pas_backend_summary_search            (PASBackendSummary *summary, const char *query);
char*              pas_backend_summary_get_summary_vcard (PASBackendSummary *summary, const char *id);

#endif /* __PAS_BACKEND_SUMMARY_H__ */
