/*
	Copyright 2000 Helix Code Inc.
*/

/* lookup.c: a simple client, part 2 */

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
  fprintf(stderr, "Usage: lookup [-f indexfile] word ...\n");
  exit(1);
}

int
main(int argc, char **argv)
{
  ibex *ib;
  GPtrArray *ans, *words;
  int opt, i;
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

  ib = ibex_open(file, FALSE);
  if (!ib)
    {
      printf("Couldn't open %s: %s\n", file, strerror(errno));
      exit(1);
    }

  words = g_ptr_array_new();
  while (argc--)
    g_ptr_array_add(words, argv[argc]);

  ans = ibex_find_all(ib, words);
  if (ans)
    {
      for (i = 0; i < ans->len; i++)
	printf("%s\n", (char *)g_ptr_array_index(ans, i));
      exit(0);
    }
  else
    {
      printf("Nope.\n");
      exit(1);
    }
}
