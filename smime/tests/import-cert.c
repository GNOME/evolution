
#include <libgnomeui/gnome-ui-init.h>
#include "e-cert-db.h"

int
main (int argc, char **argv)
{
  ECertDB *db;

  gnome_program_init ();

  g_type_init ();

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

  e_cert_db_shutdown ();
}
