/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
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
 */

#ifndef _CAMEL_FILTER_DRIVER_H
#define _CAMEL_FILTER_DRIVER_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <camel/camel-object.h>
#include <camel/camel-session.h>
#include <camel/camel-folder.h>
#include <camel/camel-uid-cache.h>

#define CAMEL_FILTER_DRIVER_TYPE         (camel_filter_driver_get_type ())
#define CAMEL_FILTER_DRIVER(obj)         CAMEL_CHECK_CAST (obj, CAMEL_FILTER_DRIVER_TYPE, CamelFilterDriver)
#define CAMEL_FILTER_DRIVER_CLASS(klass) CAMEL__CHECK_CLASS_CAST (klass, CAMEL_FILTER_DRIVER_TYPE, CamelFilterDriverClass)
#define CAMEL_IS_FILTER_DRIVER(obj)      CAMEL_CHECK_TYPE (obj, CAMEL_FILTER_DRIVER_TYPE)

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

typedef CamelFolder * (*CamelFilterGetFolderFunc) (CamelFilterDriver *driver, const char *uri,
						   void *data, CamelException *ex);
/* report status */
typedef void (CamelFilterStatusFunc) (CamelFilterDriver *driver, enum camel_filter_status_t status,
				      int pc, const char *desc, void *data);

typedef void (CamelFilterShellFunc)      (CamelFilterDriver *driver, int argc, char **argv, void *data);
typedef void (CamelFilterPlaySoundFunc)  (CamelFilterDriver *driver, const char *filename, void *data);
typedef void (CamelFilterSystemBeepFunc) (CamelFilterDriver *driver, void *data);

CamelType          camel_filter_driver_get_type (void);
CamelFilterDriver  *camel_filter_driver_new     (CamelSession *);

/* modifiers */
void camel_filter_driver_set_logfile          (CamelFilterDriver *d, FILE *logfile);

void camel_filter_driver_set_status_func      (CamelFilterDriver *d, CamelFilterStatusFunc *func, void *data);
void camel_filter_driver_set_shell_func       (CamelFilterDriver *d, CamelFilterShellFunc *func, void *data);
void camel_filter_driver_set_play_sound_func  (CamelFilterDriver *d, CamelFilterPlaySoundFunc *func, void *data);
void camel_filter_driver_set_system_beep_func (CamelFilterDriver *d, CamelFilterSystemBeepFunc *func, void *data);
void camel_filter_driver_set_folder_func      (CamelFilterDriver *d, CamelFilterGetFolderFunc fetcher, void *data);

void camel_filter_driver_set_default_folder   (CamelFilterDriver *d, CamelFolder *def);

void camel_filter_driver_add_rule             (CamelFilterDriver *d, const char *name, const char *match,
					       const char *action);
int  camel_filter_driver_remove_rule_by_name  (CamelFilterDriver *d, const char *name);

/*void camel_filter_driver_set_global(CamelFilterDriver *, const char *name, const char *value);*/

void camel_filter_driver_flush                (CamelFilterDriver *driver, CamelException *ex);

int  camel_filter_driver_filter_message       (CamelFilterDriver *driver, CamelMimeMessage *message,
					       CamelMessageInfo *info, const char *uri,
					       CamelFolder *source, const char *source_url,
					       const char *original_source_url, CamelException *ex);

int  camel_filter_driver_filter_mbox          (CamelFilterDriver *driver, const char *mbox,
					       const char *original_source_url, CamelException *ex);

int  camel_filter_driver_filter_folder        (CamelFilterDriver *driver, CamelFolder *folder, CamelUIDCache *cache,
					       GPtrArray *uids, gboolean remove, CamelException *ex);

#if 0
/* generate the search query/action string for a filter option */
void camel_filter_driver_expand_option (CamelFilterDriver *d, GString *s, GString *action, struct filter_option *op);

/* get info about rules (options) */
int camel_filter_driver_rule_count (CamelFilterDriver *d);
struct filter_option *camel_filter_driver_rule_get (CamelFilterDriver *d, int n);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_FILTER_DRIVER_H */
