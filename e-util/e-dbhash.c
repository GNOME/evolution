/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   JP Rosevear (jpr@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <string.h>
#include <fcntl.h>
#ifdef HAVE_DB_185_H
#include <db_185.h>
#else
#ifdef HAVE_DB1_DB_H
#include <db1/db.h>
#else
#include <db.h>
#endif
#endif
#include "md5-utils.h"
#include "e-dbhash.h"

struct _EDbHashPrivate 
{
	DB *db;
};

EDbHash *
e_dbhash_new (const char *filename)
{
	EDbHash *edbh;
	DB *db;

	/* Attempt to open the database */
	db = dbopen (filename, O_RDWR, 0666, DB_HASH, NULL);
	if (db == NULL) {
		db = dbopen (filename, O_RDWR | O_CREAT, 0666, DB_HASH, NULL);

		if (db == NULL)
			return NULL;
	}
	
	edbh = g_new (EDbHash, 1);
	edbh->priv = g_new (EDbHashPrivate, 1);
	edbh->priv->db = db;
	
	return edbh;
}

static void
string_to_dbt(const char *str, DBT *dbt)
{
	dbt->data = (void*)str;
	dbt->size = strlen (str) + 1;
}

void 
e_dbhash_add (EDbHash *edbh, const gchar *key, const gchar *data)
{
	DB *db;
	DBT dkey;
	DBT ddata;
	guchar local_hash[16];

	g_return_if_fail (edbh != NULL);
	g_return_if_fail (edbh->priv != NULL);
	g_return_if_fail (edbh->priv->db != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (data != NULL);

	db = edbh->priv->db;
	
	/* Key dbt */
	string_to_dbt (key, &dkey);

	/* Data dbt */
	md5_get_digest (data, strlen (data), local_hash);
	string_to_dbt (local_hash, &ddata);

	/* Add to database */
	db->put (db, &dkey, &ddata, 0);
}

void 
e_dbhash_remove (EDbHash *edbh, const char *key)
{
	DB *db;
	DBT dkey;
	
	g_return_if_fail (edbh != NULL);
	g_return_if_fail (edbh->priv != NULL);
	g_return_if_fail (key != NULL);

	db = edbh->priv->db;
	
	/* Key dbt */
	string_to_dbt (key, &dkey);

	/* Remove from database */
	db->del (db, &dkey, 0);
}

void 
e_dbhash_foreach_key (EDbHash *edbh, EDbHashFunc func, gpointer user_data)
{
	DB *db;
	DBT dkey;
	DBT ddata;
	int db_error = 0;
	
	g_return_if_fail (edbh != NULL);
	g_return_if_fail (edbh->priv != NULL);
	g_return_if_fail (func != NULL);

	db = edbh->priv->db;

	db_error = db->seq(db, &dkey, &ddata, R_FIRST);

	while (db_error == 0) {
		(*func) ((const char *)dkey.data, user_data);

		db_error = db->seq(db, &dkey, &ddata, R_NEXT);
	}
}

EDbHashStatus
e_dbhash_compare (EDbHash *edbh, const char *key, const char *compare_data)
{
	DB *db;
	DBT dkey;
	DBT ddata;
	guchar compare_hash[16];
	
	g_return_val_if_fail (edbh != NULL, FALSE);
	g_return_val_if_fail (edbh->priv != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (compare_hash != NULL, FALSE);

	db = edbh->priv->db;
	
	/* Key dbt */
	string_to_dbt (key, &dkey);

	/* Lookup in database */
	memset (&ddata, 0, sizeof (DBT));
	db->get (db, &dkey, &ddata, 0);
	
	/* Compare */
	if (ddata.data) {
		md5_get_digest (compare_data, strlen (compare_data), compare_hash);
		
		if (memcmp (ddata.data, compare_hash, sizeof (guchar) * 16))
			return E_DBHASH_STATUS_DIFFERENT;
	} else {
		return E_DBHASH_STATUS_NOT_FOUND;
	}
	
	return E_DBHASH_STATUS_SAME;
}

void
e_dbhash_write (EDbHash *edbh)
{
	DB *db;
	
	g_return_if_fail (edbh != NULL);
	g_return_if_fail (edbh->priv != NULL);

	db = edbh->priv->db;
	
	/* Flush database to disk */
	db->sync (db, 0);
}

void 
e_dbhash_destroy (EDbHash *edbh)
{
	DB *db;
	
	g_return_if_fail (edbh != NULL);
	g_return_if_fail (edbh->priv != NULL);

	db = edbh->priv->db;
	
	/* Close datbase */
	db->close (db);
	
	g_free (edbh->priv);
	g_free (edbh);
}


