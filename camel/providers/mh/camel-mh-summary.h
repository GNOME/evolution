/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _CAMEL_MH_SUMMARY_H
#define _CAMEL_MH_SUMMARY_H

#include <camel/camel-folder-summary.h>
#include <camel/camel-exception.h>
#include <libibex/ibex.h>

#define CAMEL_MH_SUMMARY(obj)	CAMEL_CHECK_CAST (obj, camel_mh_summary_get_type (), CamelMhSummary)
#define CAMEL_MH_SUMMARY_CLASS(klass)	CAMEL_CHECK_CLASS_CAST (klass, camel_mh_summary_get_type (), CamelMhSummaryClass)
#define IS_CAMEL_MH_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_mh_summary_get_type ())

typedef struct _CamelMhSummary	CamelMhSummary;
typedef struct _CamelMhSummaryClass	CamelMhSummaryClass;

struct _CamelMhSummary {
	CamelFolderSummary parent;
	struct _CamelMhSummaryPrivate *priv;

	char *mh_path;
	ibex *index;
};

struct _CamelMhSummaryClass {
	CamelFolderSummaryClass parent_class;

	/* virtual methods */

	/* signals */
};

CamelType	 camel_mh_summary_get_type	(void);
CamelMhSummary	*camel_mh_summary_new	(const char *filename, const char *mhdir, ibex *index);

/* methods */
int		camel_mh_summary_load(CamelMhSummary * mhs, int forceindex);
int		camel_mh_summary_check(CamelMhSummary * mhs, int forceindex);
int		camel_mh_summary_add(CamelMhSummary * mhs, const char *name, int forceindex);
int		camel_mh_summary_sync(CamelMhSummary * mhs, int expunge, CamelException *ex);

#endif /* ! _CAMEL_MH_SUMMARY_H */

