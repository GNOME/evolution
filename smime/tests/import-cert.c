
#include "e-cert-db.h"

int
main (int argc, char **argv)
{
  ECertDB *db;

  g_type_init ();

  if (SECSuccess != NSS_InitReadWrite ("/home/toshok/.mozilla/default/xuvq7jx3.slt")) {
    g_error ("NSS_InitReadWrite failed");
  }

  STAN_LoadDefaultNSS3TrustDomain();

  db = e_cert_db_peek ();

  printf ("default_trust_domain = %p\n", STAN_GetDefaultTrustDomain());
  printf ("default_crypto_context = %p\n", STAN_GetDefaultCryptoContext());

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
