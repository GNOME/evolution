/*

	   Encode or decode file as MIME base64 (RFC 1341)

			    by John Walker
		       http://www.fourmilab.ch/

		This program is in the public domain.

*/

#define REVDATE "11th August 1997"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define TRUE  1
#define FALSE 0

#define LINELEN 72		      /* Encoded line length (max 76) */

typedef unsigned char byte;	      /* Byte type */

static FILE *fi = stdin;	      /* Input file */
static FILE *fo = stdout;	      /* Output file */
static byte iobuf[256]; 	      /* I/O buffer */
static int iolen = 0;		      /* Bytes left in I/O buffer */
static int iocp = 256;		      /* Character removal pointer */
static int ateof = FALSE;	      /* EOF encountered */
static byte dtable[256];	      /* Encode / decode table */
static int linelength = 0;	      /* Length of encoded output line */
static char eol[] = "\r\n";           /* End of line sequence */
static int errcheck = TRUE;	      /* Check decode input for errors ? */

/*  INBUF  --  Fill input buffer with data  */

static int inbuf(void)
{
    int l;

    if (ateof) {
	return FALSE;
    }
    l = fread(iobuf, 1, 256, fi);     /* Read input buffer */
    if (l <= 0) {
	if (ferror(fi)) {
	    exit(1);
	}
	ateof = TRUE;
	return FALSE;
    }
    iolen = l;
    iocp = 0;
    return TRUE;
}

/*  INCHAR  --	Return next character from input  */

static int inchar(void)
{
    if (iocp >= iolen) {
       if (!inbuf()) {
	  return EOF;
	}
    }

    return iobuf[iocp++];
}

/*  OCHAR  --  Output an encoded character, inserting line breaks
	       where required.	*/

static void ochar(int c)
{
    if (linelength >= LINELEN) {
	if (fputs(eol, fo) == EOF) {
	    exit(1);
	}
	linelength = 0;
    }
    if (putc(((byte) c), fo) == EOF) {
	exit(1);
    }
    linelength++;
}

/*  ENCODE  --	Encode binary file into base64.  */

static void encode(void)
{
    int i, hiteof = FALSE;

    /*	Fill dtable with character encodings.  */

    for (i = 0; i < 26; i++) {
        dtable[i] = 'A' + i;
        dtable[26 + i] = 'a' + i;
    }
    for (i = 0; i < 10; i++) {
        dtable[52 + i] = '0' + i;
    }
    dtable[62] = '+';
    dtable[63] = '/';

    while (!hiteof) {
	byte igroup[3], ogroup[4];
	int c, n;

	igroup[0] = igroup[1] = igroup[2] = 0;
	for (n = 0; n < 3; n++) {
	    c = inchar();
	    if (c == EOF) {
		hiteof = TRUE;
		break;
	    }
	    igroup[n] = (byte) c;
	}
	if (n > 0) {
	    ogroup[0] = dtable[igroup[0] >> 2];
	    ogroup[1] = dtable[((igroup[0] & 3) << 4) | (igroup[1] >> 4)];
	    ogroup[2] = dtable[((igroup[1] & 0xF) << 2) | (igroup[2] >> 6)];
	    ogroup[3] = dtable[igroup[2] & 0x3F];

            /* Replace characters in output stream with "=" pad
	       characters if fewer than three characters were
	       read from the end of the input stream. */

	    if (n < 3) {
                ogroup[3] = '=';
		if (n < 2) {
                    ogroup[2] = '=';
		}
	    }
	    for (i = 0; i < 4; i++) {
		ochar(ogroup[i]);
	    }
	}
    }
    if (fputs(eol, fo) == EOF) {
	exit(1);
    }
}

/*  INSIG  --  Return next significant input  */

static int insig(void)
{
    int c;

    /*CONSTANTCONDITION*/
    while (TRUE) {
	c = inchar();
        if (c == EOF || (c > ' ')) {
	    return c;
	}
    }
    /*NOTREACHED*/
}

/*  DECODE  --	Decode base64.	*/

static void decode(void)
{
    int i;

    for (i = 0; i < 255; i++) {
	dtable[i] = 0x80;
    }
    for (i = 'A'; i <= 'Z'; i++) {
        dtable[i] = 0 + (i - 'A');
    }
    for (i = 'a'; i <= 'z'; i++) {
        dtable[i] = 26 + (i - 'a');
    }
    for (i = '0'; i <= '9'; i++) {
        dtable[i] = 52 + (i - '0');
    }
    dtable['+'] = 62;
    dtable['/'] = 63;
    dtable['='] = 0;

    /*CONSTANTCONDITION*/
    while (TRUE) {
	byte a[4], b[4], o[3];

	for (i = 0; i < 4; i++) {
	    int c = insig();

	    if (c == EOF) {
		if (errcheck && (i > 0)) {
                    fprintf(stderr, "Input file incomplete.\n");
		    exit(1);
		}
		return;
	    }
	    if (dtable[c] & 0x80) {
		if (errcheck) {
                    fprintf(stderr, "Illegal character '%c' in input file.\n", c);
		    exit(1);
		}
		/* Ignoring errors: discard invalid character. */
		i--;
		continue;
	    }
	    a[i] = (byte) c;
	    b[i] = (byte) dtable[c];
	}
	o[0] = (b[0] << 2) | (b[1] >> 4);
	o[1] = (b[1] << 4) | (b[2] >> 2);
	o[2] = (b[2] << 6) | b[3];
        i = a[2] == '=' ? 1 : (a[3] == '=' ? 2 : 3);
	if (fwrite(o, i, 1, fo) == EOF) {
	    exit(1);
	}
	if (i < 3) {
	    return;
	}
    }
}

/*  USAGE  --  Print how-to-call information.  */

static void usage(char *pname)
{
    fprintf(stderr, "%s  --  Encode/decode file as base64.  Call:\n", pname);
    fprintf(stderr,
    "            %s [-e[ncode] / -d[ecode]] [-n] [infile] [outfile]\n", pname);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "           -D         Decode base64 encoded file\n");
    fprintf(stderr, "           -E         Encode file into base64\n");
    fprintf(stderr, "           -N         Ignore errors when decoding\n");
    fprintf(stderr, "           -U         Print this message\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "by John Walker\n");
    fprintf(stderr, "   WWW:    http://www.fourmilab.ch/\n");
}

/*  Main program  */

int main(int argc, char *argv[])
{
    int i, f = 0, decoding = FALSE;
    char *cp, opt;

    for (i = 1; i < argc; i++) {
	cp = argv[i];
        if (*cp == '-') {
	    opt = *(++cp);
	    if (islower(opt)) {
		opt = toupper(opt);
	    }
	    switch (opt) {

                case 'D':             /* -D  Decode */
		    decoding = TRUE;
		    break;

                case 'E':             /* -E  Encode */
		    decoding = FALSE;
		    break;

                case 'N':             /* -N  Suppress error checking */
		    errcheck = FALSE;
		    break;

                case 'U':             /* -U  Print how-to-call information */
                case '?':
		    usage(argv[0]);
		    return 0;
	   }
	} else {
	    switch (f) {

		/** Warning!  On systems which distinguish text mode and
		    binary I/O (MS-DOS, Macintosh, etc.) the modes in these
		    open statements will have to be made conditional based
		    upon whether an encode or decode is being done, which
                    will have to be specified earlier.  But it's worse: if
		    input or output is from standard input or output, the 
		    mode will have to be changed on the fly, which is
                    generally system and compiler dependent.  'Twasn't me
                    who couldn't conform to Unix CR/LF convention, so 
                    don't ask me to write the code to work around
                    Apple and Microsoft's incompatible standards. **/

		case 0:
                    if (strcmp(cp, "-") != 0) {
                        if ((fi = fopen(cp, "r")) == NULL) {
                            fprintf(stderr, "Cannot open input file %s\n", cp);
			    return 2;
			}
		    }
		    f++;
		    break;

		case 1:
                    if (strcmp(cp, "-") != 0) {
                        if ((fo = fopen(cp, "w")) == NULL) {
                            fprintf(stderr, "Cannot open output file %s\n", cp);
			    return 2;
			}
		    }
		    f++;
		    break;

		default:
                    fprintf(stderr, "Too many file names specified.\n");
		    usage(argv[0]);
		    return 2;
	    }
       }
    }

    if (decoding) {
       decode();
    } else {
       encode();
    }
    return 0;
}
