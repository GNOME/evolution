/*
	Copyright 2000 Helix Code Inc.
*/

#ifndef _IBEX_H
#define _IBEX_H

#include <sys/types.h>
#include <glib.h>

struct ibex;
typedef struct ibex ibex;

/* All functions that can fail set errno and return NULL or -1 on
 * failure.
 */

/* Open the named ibex index file. If CREATE is true, create the file
 * if it doesn't already exist.
 */
ibex *ibex_open(char *file, gboolean create);

/* Write the ibex to disk. */
int ibex_write(ibex *ib);

/* Write the ibex to disk if it has changed, and free all memory
 * associated with it.
 */
int ibex_close(ibex *ib);



/* Index the named file. (If the FILENAME is already in the index,
 * remove the old copy.
 */
int ibex_index_file(ibex *ib, char *filename);

/* Index LEN bytes off FD, using NAME as the filename in the index.
 * (If NAME already exists in the index, this adds more data to it.)
 */
int ibex_index_fd(ibex *ib, char *name, int fd, size_t len);

/* Like ibex_index_fd, but with a buffer rather than a file descriptor.
 * The buffer does not need to be '\0'-terminated. If UNREAD is not
 * NULL, then the indexer won't assume that the buffer ends on a word
 * boundary, and will return (in UNREAD) the number of bytes from the
 * end of the buffer that it didn't use, if any.
 */
int ibex_index_buffer(ibex *ib, char *name, char *buffer,
		      size_t len, size_t *unread);

/* Remove entries for a given file from the index. (Most of the removal
 * isn't actually done until the file is written out to disk, so this
 * is very fast.)
 */
void ibex_unindex(ibex *ib, char *name);

/* Rename a file in the index. (This is also fast.) */
void ibex_rename(ibex *ib, char *oldfilename, char *newfilename);



/* Find a word in the index. Returns an array of strings: the caller
 * should free the array, but should not free or modify the strings.
 */
GPtrArray *ibex_find(ibex *ib, char *word);

/* Find if a word is contained in a specific name reference.
 */
gboolean ibex_find_name(ibex *ib, char *name, char *word);

/* Return all the files containing all of the words in the given
 * array. Returned data is like with ibex_find.
 */
GPtrArray *ibex_find_all(ibex *ib, GPtrArray *words);

#endif /* ! _IBEX_H */

