/*
	Copyright 2000 Helix Code Inc.
*/
/* mkindex.c: a simple client, part 1 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ibex.h"

extern int optind;
extern char *optarg;

static void
usage(void)
{
  fprintf(stderr, "Usage: mkindex [-f indexfile] file ...\n");
  exit(1);
}

int
main(int argc, char **argv)
{
  ibex *ib;
  int opt;
  char *file = "INDEX";

  while ((opt = getopt(argc, argv, "f:")) != -1)
    {
      switch (opt)
	{
	case 'f':
	  file = optarg;
	  break;

	default:
	  usage();
	  break;
	}
    }
  argc -= optind;
  argv += optind;

  if (argc == 0)
    usage();

  ib = ibex_open(file, TRUE);
  if (!ib)
    {
      fprintf(stderr, "Couldn't open index file %s: %s\n",
	      file, strerror(errno));
      exit(1);
    }

  while (argc--)
    {
      if (ibex_index_file(ib, argv[argc]) == -1)
	{
	  fprintf(stderr, "Couldn't index %s: %s\n", argv[argc],
		 strerror(errno));
	  exit(1);
	}
    }


  if (ibex_close(ib) != 0)
    {
      fprintf(stderr, "Failed to write index file %s: %s\n",
	      file, strerror(errno));
      exit(1);
    }
  exit(0);
}
