/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *           Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#ifndef _CAMEL_FILTER_DRIVER_H
#define _CAMEL_FILTER_DRIVER_H

#include <camel/camel-object.h>
#include <camel/camel-session.h>
#include <camel/camel-folder.h>

#define CAMEL_FILTER_DRIVER_TYPE   (camel_filter_driver_get_type())
#define CAMEL_FILTER_DRIVER(obj)         CAMEL_CHECK_CAST (obj, camel_filter_driver_get_type (), CamelFilterDriver)
#define CAMEL_FILTER_DRIVER_CLASS(klass) CAMEL__CHECK_CLASS_CAST (klass, camel_filter_driver_get_type (), CamelFilterDriverClass)
#define CAMEL_IS_FILTER_DRIVER(obj)      CAMEL_CHECK_TYPE (obj, camel_filter_driver_get_type ())

typedef struct _CamelFilterDriver      CamelFilterDriver;
typedef struct _CamelFilterDriverClass CamelFilterDriverClass;

struct _CamelFilterDriver {
	CamelObject parent;

	struct _CamelFilterDriverPrivate *priv;
};

struct _CamelFilterDriverClass {
	CamelObjectClass parent_class;
};

/* FIXME: this maybe should change... */
/* type of status for a status report */
enum camel_filter_status_t {
	CAMEL_FILTER_STATUS_NONE,
	CAMEL_FILTER_STATUS_START,	/* start of new message processed */
	CAMEL_FILTER_STATUS_ACTION,	/* an action performed */
	CAMEL_FILTER_STATUS_PROGRESS,	/* (an) extra update(s), if its taking longer to process */
	CAMEL_FILTER_STATUS_END,	/* end of message */
};

typedef CamelFolder * (*CamelFilterGetFolderFunc) (CamelFilterDriver *, const char *uri, void *data, CamelException *ex);
/* report status */
typedef void (CamelFilterStatusFunc)(CamelFilterDriver *driver, enum camel_filter_status_t status, int pc, const char *desc, void *data);

guint         camel_filter_driver_get_type (void);
CamelFilterDriver  *camel_filter_driver_new     (CamelFilterGetFolderFunc fetcher, void *data);

/* modifiers */
void    camel_filter_driver_set_logfile         (CamelFilterDriver *d, FILE *logfile);
void	camel_filter_driver_set_status_func     (CamelFilterDriver *d, CamelFilterStatusFunc *func,
						 void *data);
void	camel_filter_driver_set_default_folder  (CamelFilterDriver *d, CamelFolder *def);
void 	camel_filter_driver_add_rule		(CamelFilterDriver *d, const char *name, const char *match,
						 const char *action);

/*void camel_filter_driver_set_global(CamelFilterDriver *, const char *name, const char *value);*/

void    camel_filter_driver_filter_message      (CamelFilterDriver *driver, CamelMimeMessage *message,
						 CamelMessageInfo *info, const char *uri,
						 CamelFolder *source, const char *source_url,
						 CamelException *ex);
void    camel_filter_driver_filter_mbox         (CamelFilterDriver *driver, const char *mbox,
						 CamelException *ex);
void    camel_filter_driver_filter_folder       (CamelFilterDriver *driver, CamelFolder *folder,
						 GPtrArray *uids, gboolean remove, CamelException *ex);

#if 0
/* generate the search query/action string for a filter option */
void camel_filter_driver_expand_option (CamelFilterDriver *d, GString *s, GString *action, struct filter_option *op);

/* get info about rules (options) */
int camel_filter_driver_rule_count (CamelFilterDriver *d);
struct filter_option *camel_filter_driver_rule_get (CamelFilterDriver *d, int n);
#endif

#endif /* ! _CAMEL_FILTER_DRIVER_H */
