
#ifndef _CAMEL_IMAPP_DRIVER_H
#define _CAMEL_IMAPP_DRIVER_H

#include <camel/camel-object.h>
#include "camel-imapp-stream.h"
#include <libedataserver/e-msgport.h>

#define CAMEL_IMAPP_DRIVER_TYPE     (camel_imapp_driver_get_type ())
#define CAMEL_IMAPP_DRIVER(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_IMAPP_DRIVER_TYPE, CamelIMAPPDriver))
#define CAMEL_IMAPP_DRIVER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_IMAPP_DRIVER_TYPE, CamelIMAPPDriverClass))
#define CAMEL_IS_IMAP_DRIVER(o)    (CAMEL_CHECK_TYPE((o), CAMEL_IMAPP_DRIVER_TYPE))

typedef struct _CamelIMAPPDriver CamelIMAPPDriver;
typedef struct _CamelIMAPPDriverClass CamelIMAPPDriverClass;

typedef struct _CamelIMAPPFetch CamelIMAPPFetch;

typedef int (*CamelIMAPPDriverFunc)(struct _CamelIMAPPDriver *driver, void *data);
typedef struct _CamelSasl * (*CamelIMAPPSASLFunc)(struct _CamelIMAPPDriver *driver, void *data);
typedef void (*CamelIMAPPLoginFunc)(struct _CamelIMAPPDriver *driver, char **login, char **pass, void *data);

typedef void (*CamelIMAPPFetchFunc)(struct _CamelIMAPPDriver *driver, CamelIMAPPFetch *);

struct _CamelIMAPPFetch {
	struct _CamelIMAPPFetch *next;
	struct _CamelIMAPPFetch *prev;

	CamelStream *body;	/* the content fetched */

	struct _CamelIMAPPFolder *folder;
	char *uid;
	char *section;

	CamelIMAPPFetchFunc done;
	void *data;
};

struct _CamelMimeMessage;

struct _CamelIMAPPDriver {
	CamelObject parent_object;

	struct _CamelIMAPPEngine *engine;

	struct _CamelIMAPPFolder *folder;

	/* current folder stuff */
	GPtrArray *summary;
	guint32 uidvalidity;
	guint32 exists;
	guint32 recent;
	guint32 unseen;
	guint32 permanentflags;

	/* list stuff */
	GPtrArray *list_result;
	GSList *list_commands;
	guint32 list_flags;

	/* sem_t list_sem; for controlled access to list variables */

	/* this is so the node is always in a list - easier exception management */
	EDList body_fetch;
	EDList body_fetch_done;

	/* factory to get an appropriate sasl mech */
	CamelIMAPPSASLFunc get_sasl;
	void *get_sasl_data;

	/* callbacks, get login username/pass */
	CamelIMAPPLoginFunc get_login;
	void *get_login_data;
};

struct _CamelIMAPPDriverClass {
	CamelObjectClass parent_class;
};

CamelType       	camel_imapp_driver_get_type (void);

CamelIMAPPDriver *	camel_imapp_driver_new(CamelIMAPPStream *stream);

void			camel_imapp_driver_set_sasl_factory(CamelIMAPPDriver *id, CamelIMAPPSASLFunc get_sasl, void *sasl_data);
void			camel_imapp_driver_set_login_query(CamelIMAPPDriver *id, CamelIMAPPLoginFunc get_login, void *login_data);

void			camel_imapp_driver_login(CamelIMAPPDriver *id);

void			camel_imapp_driver_select(CamelIMAPPDriver *id, struct _CamelIMAPPFolder *folder);
void			camel_imapp_driver_update(CamelIMAPPDriver *id, struct _CamelIMAPPFolder *folder);
void			camel_imapp_driver_sync(CamelIMAPPDriver *id, gboolean expunge, struct _CamelIMAPPFolder *folder);

struct _CamelStream *	camel_imapp_driver_fetch(CamelIMAPPDriver *id, struct _CamelIMAPPFolder *folder, const char *uid, const char *body);

GPtrArray *		camel_imapp_driver_list(CamelIMAPPDriver *id, const char *name, guint32 flags);

struct _CamelStream *camel_imapp_driver_get(CamelIMAPPDriver *id, struct _CamelIMAPPFolder *folder, const char *uid);
void camel_imapp_driver_append(CamelIMAPPDriver *id, struct _CamelIMAPPFolder *folder, struct _CamelDataWrapper *);

#endif
