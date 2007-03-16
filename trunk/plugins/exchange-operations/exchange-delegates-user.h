/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2002-2004 Novell, Inc. */

#ifndef __EXCHANGE_DELEGATES_USER_H__
#define __EXCHANGE_DELEGATES_USER_H__

#include <exchange-types.h>
#include <e2k-security-descriptor.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EXCHANGE_TYPE_DELEGATES_USER			(exchange_delegates_user_get_type ())
#define EXCHANGE_DELEGATES_USER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EXCHANGE_TYPE_DELEGATES_USER, ExchangeDelegatesUser))
#define EXCHANGE_DELEGATES_USER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EXCHANGE_TYPE_DELEGATES_USER, ExchangeDelegatesUserClass))
#define EXCHANGE_IS_DELEGATES_USER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXCHANGE_TYPE_DELEGATES_USER))
#define EXCHANGE_IS_DELEGATES_USER_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), EXCHANGE_TYPE_DELEGATES_USER))


typedef struct _ExchangeDelegatesUser        ExchangeDelegatesUser;
typedef struct _ExchangeDelegatesUserPrivate ExchangeDelegatesUserPrivate;
typedef struct _ExchangeDelegatesUserClass   ExchangeDelegatesUserClass;

enum {
	EXCHANGE_DELEGATES_CALENDAR,
	EXCHANGE_DELEGATES_TASKS,
	EXCHANGE_DELEGATES_INBOX,
	EXCHANGE_DELEGATES_CONTACTS,
	EXCHANGE_DELEGATES_LAST
};

struct _ExchangeDelegatesUser {
	GObject parent;

	char *display_name, *dn;
	GByteArray *entryid;

	E2kSid *sid;
	E2kPermissionsRole role[EXCHANGE_DELEGATES_LAST];
	gboolean see_private;
};

struct _ExchangeDelegatesUserClass {
	GObjectClass parent_class;

	/* signals */
	void (*edited) (ExchangeDelegatesUser *, gpointer);
};



GType    exchange_delegates_user_get_type (void);

ExchangeDelegatesUser *exchange_delegates_user_new         (const char            *display_name);
ExchangeDelegatesUser *exchange_delegates_user_new_from_gc (E2kGlobalCatalog      *gc,
							    const char            *email,
							    GByteArray            *creator_entryid);

gboolean               exchange_delegates_user_edit        (ExchangeDelegatesUser *user,
							    GtkWidget             *parent_window);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EXCHANGE_DELEGATES_USER_H__ */
