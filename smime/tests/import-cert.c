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

#include <gtk/gtk.h>
#include <libgnomeui/gnome-ui-init.h>

#include "e-cert-db.h"
#include "e-pkcs12.h"

int
main (int argc, char **argv)
{
  ECertDB *db;
  EPKCS12 *pkcs12;

  gnome_program_init("import-cert-test", "0.0", LIBGNOMEUI_MODULE, argc, argv, NULL);

  db = e_cert_db_peek ();

  if (!e_cert_db_import_certs_from_file (db, "ca.crt", E_CERT_CA, NULL /* XXX */)) {
    g_warning ("CA cert import failed");
  }

  if (!e_cert_db_import_certs_from_file (db, "", E_CERT_CONTACT, NULL /* XXX */)) {
    g_warning ("contact cert import failed");
  }

  if (!e_cert_db_import_certs_from_file (db, "", E_CERT_SITE, NULL /* XXX */)) {
    g_warning ("server cert import failed");
  }

  pkcs12 = e_pkcs12_new ();
  if (!e_pkcs12_import_from_file (pkcs12, "newcert.p12", NULL /* XXX */)) {
    g_warning ("PKCS12 import failed");
  }

  e_cert_db_shutdown ();

  return 0;
}
