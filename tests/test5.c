/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* test for the RFC 2047 decoder */

#include <string.h>
#include <unicode.h>

#include "gmime-utils.h"
#include "stdio.h"
#include "camel-log.h"
#include "camel-mime-message.h"
#include "camel-mime-part.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel.h"
#include "gmime-rfc2047.h"

#define TERMINAL_CHARSET "UTF-8"

/* 
 * Info on many unicode issues, including, utf-8 xterms from :
 * 
 *   http://www.cl.cam.ac.uk/~mgk/unicode.html
 *
 */

const char *tests[] = 
{ 
/* these strings come from RFC 2047. Ought to add a few torture cases here. */
  "=?US-ASCII?Q?Keith_Moore?= <moore@cs.utk.edu>",
  "=?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?= <keld@dkuug.dk>",
  "=?ISO-8859-1?Q?Andr=E9?= Pirard <PIRARD@vm1.ulg.ac.be>",
  "=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?= =?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=",
  "=?ISO-8859-1?Q?Olle_J=E4rnefors?= <ojarnef@admin.kth.se>",
  "=?ISO-8859-1?Q?Patrik_F=E4ltstr=F6m?= <paf@nada.kth.se>",
  "Nathaniel Borenstein <nsb@thumper.bellcore.com> (=?iso-8859-8?b?7eXs+SDv4SDp7Oj08A==?=)",
  "",
  "(=?ISO-8859-1?Q?a?=)",     /* should be displayed as           (a)   */
  "(=?ISO-8859-1?Q?a?= b)",                                  /*   (a b) */
  "(=?ISO-8859-1?Q?a?= =?ISO-8859-1?Q?b?=)",                 /*   (ab)  */
  "(=?ISO-8859-1?Q?a?=  =?ISO-8859-1?Q?b?=)",                /*   (ab)  */
  "(=?ISO-8859-1?Q?a?= \n=?ISO-8859-1?Q?b?=)",               /*   (ab)  */
  "(=?ISO-8859-1?Q?a_b?=)",                                  /*   (a b) */
  "(=?ISO-8859-1?Q?a?= =?ISO-8859-2?Q?_b?=)",                /*   (ab)  */
  NULL
};
  

int
main (int argc, char**argv)
{      
	const char **b = tests;
	while (*b) {
		printf("%s\n", gmime_rfc2047_decode(*b, TERMINAL_CHARSET));
		b++;
	}

	return 0;

}
