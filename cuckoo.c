/**
 * @file cuckoo.c
 *
 * requires libconfig
 *
 * Created by Paul Chambers on 5/3/21.
 * MIT Licensed
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#include <libconfig.h>

int main( int argc, char * argv[], void * envp )
{
    char * myName = strrchr( argv[0], '/') + 1;
    if ( myName - 1 == NULL )
    {
        myName = argv[0];
    }
    openlog( myName, LOG_PID, LOG_USER );
    char * wd = getcwd( NULL, 0 );
    if ( wd != NULL )
    {
        syslog( LOG_INFO, "wd: \"%s\"", wd );
    }

    for ( int i = 0; i < argc; i++ )
    {
        syslog( LOG_INFO, "%2d: \"%s\"", i, argv[i] );
    }
    closelog();

    /* clean up */
    if (wd != NULL)
    {
        free( wd );
    }
}
