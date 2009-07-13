/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EXCHANGE_DELEGATES_USER_H__
#define __EXCHANGE_DELEGATES_USER_H__

#include <exchange-types.h>
#include <e2k-security-descriptor.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

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

	gchar *display_name, *dn;
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

ExchangeDelegatesUser *exchange_delegates_user_new         (const gchar            *display_name);
ExchangeDelegatesUser *exchange_delegates_user_new_from_gc (E2kGlobalCatalog      *gc,
							    const gchar            *email,
							    GByteArray            *creator_entryid);

gboolean  exchange_delegates_user_edit (ExchangeAccount  *account, ExchangeDelegatesUser *user,
					GtkWidget *parent_window);

G_END_DECLS

#endif /* __EXCHANGE_DELEGATES_USER_H__ */
