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

typedef enum _e_account_item_t {
	E_ACCOUNT_ID_NAME,
	E_ACCOUNT_ID_ADDRESS,
	E_ACCOUNT_ID_REPLY_TO,
	E_ACCOUNT_ID_ORGANIZATION,
	E_ACCOUNT_ID_SIGNATURE,

	E_ACCOUNT_SOURCE_URL,
	E_ACCOUNT_SOURCE_KEEP_ON_SERVER,
	E_ACCOUNT_SOURCE_AUTO_CHECK,
	E_ACCOUNT_SOURCE_AUTO_CHECK_TIME,
	E_ACCOUNT_SOURCE_SAVE_PASSWD,

	E_ACCOUNT_TRANSPORT_URL,
	E_ACCOUNT_TRANSPORT_SAVE_PASSWD,

	E_ACCOUNT_DRAFTS_FOLDER_URI,
	E_ACCOUNT_SENT_FOLDER_URI,

	E_ACCOUNT_CC_ALWAYS,
	E_ACCOUNT_CC_ADDRS,

	E_ACCOUNT_BCC_ALWAYS,
	E_ACCOUNT_BCC_ADDRS,

	E_ACCOUNT_PGP_KEY,
	E_ACCOUNT_PGP_ENCRYPT_TO_SELF,
	E_ACCOUNT_PGP_ALWAYS_SIGN,
	E_ACCOUNT_PGP_NO_IMIP_SIGN,
	E_ACCOUNT_PGP_ALWAYS_TRUST,

	E_ACCOUNT_SMIME_SIGN_KEY,
	E_ACCOUNT_SMIME_ENCRYPT_KEY,
	E_ACCOUNT_SMIME_SIGN_DEFAULT,
	E_ACCOUNT_SMIME_ENCRYPT_TO_SELF,
	E_ACCOUNT_SMIME_ENCRYPE_DEFAULT,

	E_ACCOUNT_ITEM_LAST
} e_account_item_t;

typedef enum _e_account_access_t {
	E_ACCOUNT_ACCESS_WRITE = 1<<0,
} e_account_access_t;

typedef struct _EAccountIdentity {
	char *name;
	char *address;
	char *reply_to;
	char *organization;
	char *sig_uid;
} EAccountIdentity;

typedef struct _EAccountService {
	char *url;
	gboolean keep_on_server;
	gboolean auto_check;
	int auto_check_time;
	gboolean save_passwd;
} EAccountService;

typedef struct _EAccount {
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
gboolean  e_account_set_from_xml (EAccount   *account, const char *xml);
void      e_account_import       (EAccount   *dest, EAccount   *src);
char     *e_account_to_xml       (EAccount   *account);
char     *e_account_uid_from_xml (const char *xml);

#if 0
const char *e_account_get_string(EAccount *, e_account_item_t type);
int e_account_get_int(EAccount *, e_account_item_t type);
gboolean e_account_get_bool(EAccount *, e_account_item_t type);

void e_account_set_string(EAccount *, e_account_item_t type, const char *);
void e_account_set_int(EAccount *, e_account_item_t type, const char *);
void e_account_set_bool(EAccount *, e_account_item_t type, const char *);
#endif

gboolean e_account_writable(EAccount *ea, e_account_item_t type);
gboolean e_account_writable_option(EAccount *ea, const char *protocol, const char *option);

#endif /* __E_ACCOUNT__ */
