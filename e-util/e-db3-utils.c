/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include "config.h"

#include "e-db3-utils.h"

#include <db.h>

#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>

#if DB_VERSION_MAJOR != 3 || \
    DB_VERSION_MINOR != 1 || \
    DB_VERSION_PATCH != 17
#error Including wrong DB3.  Need libdb 3.1.17.
#endif

static char *
get_check_filename (const char *filename)
{
	return g_strdup_printf ("%s-upgrading", filename);
}

static char *
get_copy_filename (const char *filename)
{
	return g_strdup_printf ("%s-copy", filename);
}

static int
cp_file (const char *src, const char *dest)
{
	int i;
	int o;
	char buffer[1024];
	int length;
	int place;

	i = open (src, O_RDONLY);
	if (i == -1)
		return -1;
	o = creat (dest, S_IREAD | S_IWRITE);
	if (o == -1) {
		close (i);
		return -1;
	}
	while (1) {
		length = read (i, &buffer, sizeof (buffer));

		if (length == 0)
			break;

		if (length == -1) {
			if (errno == EINTR)
				continue;
			else {
				close (i);
				close (o);
				unlink (dest);
				return -1;
			}
		}

		place = 0;
		while (length != 0) {
			int count;
			count = write (o, buffer + place, length);
			if (count == -1) {
				if (errno == EINTR)
					continue;
				else {
					close (i);
					close (o);
					unlink (dest);
					return -1;
				}
			}

			length -= count;
			place += count;
		}
	}
	if (close (i))
		return -1;
	if (close (o))
		return -1;
	return 0;
}

static int
touch_file (const char *file)
{
	int o;
	o = creat (file, S_IREAD | S_IWRITE);
	if (o == -1)
		return -1;

	if (close (o) == -1)
		return -1;

	return 0;
}

static int
resume_upgrade (const char *filename, const char *copy_filename, const char *check_filename)
{
	DB *db;
	int ret_val;

	ret_val = db_create (&db, NULL, 0);

	if (ret_val == 0)
		ret_val = cp_file (copy_filename, filename);

	if (ret_val == 0)
		ret_val = db->upgrade (db, filename, 0);

	if (ret_val == 0)
		ret_val = unlink (check_filename);
	if (ret_val == 0)
		ret_val = unlink (copy_filename);

	db->close (db, 0);

	return ret_val;
}

int
e_db3_utils_maybe_recover (const char *filename)
{
	int ret_val = 0;
	char *copy_filename;
	char *check_filename;

	copy_filename = get_copy_filename (filename);
	check_filename = get_check_filename (filename);

	if (g_file_exists (check_filename)) {
		ret_val = resume_upgrade(filename, copy_filename, check_filename);
	} else if (g_file_exists (copy_filename)) {
		unlink (copy_filename);
	}

	g_free (copy_filename);
	g_free (check_filename);
	return ret_val;
}

int
e_db3_utils_upgrade_format (const char *filename)
{
	char *copy_filename;
	char *check_filename;
	DB *db;
	int ret_val;

	ret_val = db_create (&db, NULL, 0);
	if (ret_val != 0)
		return ret_val;

	copy_filename = get_copy_filename (filename);
	check_filename = get_check_filename (filename);

	ret_val = cp_file (filename, copy_filename);

	if (ret_val == 0)
		ret_val = touch_file (check_filename);
	if (ret_val == 0)
		ret_val = db->upgrade (db, filename, 0);
	if (ret_val == 0)
		ret_val = unlink (check_filename);

	if (ret_val == 0)
		ret_val = unlink (copy_filename);

	db->close (db, 0);

	g_free (check_filename);
	g_free (copy_filename);
	return ret_val;
}
