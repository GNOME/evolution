/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __E_ACCOUNT__
#define __E_ACCOUNT__

#include <glib-object.h>

#define E_TYPE_ACCOUNT            (e_account_get_type ())
#define E_ACCOUNT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_ACCOUNT, EAccount))
#define E_ACCOUNT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_ACCOUNT, EAccountClass))
#define E_IS_ACCOUNT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_ACCOUNT))
#define E_IS_ACCOUNT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_ACCOUNT))

typedef struct {
	char *name;
	char *address;
	char *reply_to;
	char *organization;
	
	int def_signature;
	gboolean auto_signature;
} EAccountIdentity;

typedef struct {
	char *url;
	gboolean keep_on_server;
	gboolean auto_check;
	int auto_check_time;
	gboolean save_passwd;
} EAccountService;


typedef struct {
	GObject parent_object;

	char *name;
	char *uid;

	gboolean enabled;

	EAccountIdentity *id;
	EAccountService *source;
	EAccountService *transport;

	char *drafts_folder_uri, *sent_folder_uri;

	gboolean always_cc;
	char *cc_addrs;
	gboolean always_bcc;
	char *bcc_addrs;

	char *pgp_key;
	gboolean pgp_encrypt_to_self;
	gboolean pgp_always_sign;
	gboolean pgp_no_imip_sign;
	gboolean pgp_always_trust;

	char *smime_sign_key;
	char *smime_encrypt_key;
	gboolean smime_sign_default;
	gboolean smime_encrypt_to_self;
	gboolean smime_encrypt_default;
} EAccount;

typedef struct {
	GObjectClass parent_class;

} EAccountClass;


GType     e_account_get_type (void);

EAccount *e_account_new          (void);

EAccount *e_account_new_from_xml (const char *xml);

gboolean  e_account_set_from_xml (EAccount   *account,
				  const char *xml);

void      e_account_import       (EAccount   *dest,
				  EAccount   *src);

char     *e_account_to_xml       (EAccount   *account);


char     *e_account_uid_from_xml (const char *xml);


#endif /* __E_ACCOUNT__ */
