/*
	Copyright 2000 Helix Code Inc.
*/
/* index.c: high-level indexing ops */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ibex_internal.h"

/* Index a file, given its name. (Replace any previously indexed contents
 * of this file with the new contents.)
 */
int
ibex_index_file(ibex *ib, char *filename)
{
  int fd;
  int status;
  struct stat st;

  fd = open(filename, O_RDONLY);
  if (fd < 0)
    return -1;

  if (fstat(fd, &st) == -1)
    {
      close(fd);
      return -1;
    }
  if (!S_ISREG(st.st_mode))
    {
      close(fd);
      errno = EINVAL;
      return -1;
    }

  ibex_unindex(ib, filename);
  status = ibex_index_fd(ib, filename, fd, st.st_size);
  close(fd);
  return status;
}

/* Given a file descriptor and a name to refer to it by, index LEN
 * bytes of data from it.
 */
int
ibex_index_fd(ibex *ib, char *name, int fd, size_t len)
{
  char *buf;
  int off = 0, nread, status;

  buf = g_malloc(len);
  do
    {
      nread = read(fd, buf + off, len - off);
      if (nread == -1)
	{
	  g_free(buf);
	  return -1;
	}
      off += nread;
    }
  while (off != len);

  status = ibex_index_buffer(ib, name, buf, len, NULL);
  g_free(buf);

  return status;
}

void
ibex_unindex(ibex *ib, char *name)
{
  ibex_file *ibf;

  ibf = g_tree_lookup(ib->files, name);
  if (ibf)
    {
      ibf->index = -1;
      g_tree_remove(ib->files, name);
      g_ptr_array_add(ib->oldfiles, ibf);
    }
}

void
ibex_rename(ibex *ib, char *oldname, char *newname)
{
  ibex_file *ibf;

  ibf = g_tree_lookup(ib->files, oldname);
  if (ibf)
    {
      g_tree_remove(ib->files, oldname);
      g_free(ibf->name);
      ibf->name = g_strdup(newname);
      g_tree_insert(ib->files, ibf->name, ibf);
    }
}
