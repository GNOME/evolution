/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-storage.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_SUMMARY_STORAGE_H_
#define _E_SUMMARY_STORAGE_H_

#include "e-storage.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SUMMARY_STORAGE			(e_summary_storage_get_type ())
#define E_SUMMARY_STORAGE(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SUMMARY_STORAGE, ESummaryStorage))
#define E_SUMMARY_STORAGE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SUMMARY_STORAGE, ESummaryStorageClass))
#define E_IS_SUMMARY_STORAGE(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SUMMARY_STORAGE))
#define E_IS_SUMMARY_STORAGE_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SUMMARY_STORAGE))


#define E_SUMMARY_STORAGE_NAME "summary"


typedef struct _ESummaryStorage        ESummaryStorage;
typedef struct _ESummaryStoragePrivate ESummaryStoragePrivate;
typedef struct _ESummaryStorageClass   ESummaryStorageClass;

struct _ESummaryStorage {
	EStorage parent;

	ESummaryStoragePrivate *priv;
};

struct _ESummaryStorageClass {
	EStorageClass parent_class;
};


GtkType   e_summary_storage_get_type  (void);
EStorage *e_summary_storage_new       (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SUMMARY_STORAGE_H_ */
