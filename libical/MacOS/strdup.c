
#include "strdup.h"
#include <string.h>
#include <stdlib.h>


char *strdup(const char *s )
{
        char    *p;

        if ( (p = (char *) malloc( strlen( s ) + 1 )) == NULL )
                return( NULL );

        strcpy( p, s );

        return( p );
}
