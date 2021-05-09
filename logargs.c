//
// Created by paul on 5/9/21.
//

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <syslog.h>
#include <string.h>
#include <libgen.h>


int main( int argc, const char * argv[], const char * envp[] )
{
    char * myPath = strdup( argv[0] );
    char * myName = basename( myPath );

    openlog( myName, 0, LOG_USER );

    for ( int i = 0; i < argc; i++ )
    {
        if (argv[i] != NULL)
        { syslog( LOG_INFO, "argv[%d] = \'%s\'", i, argv[i] ); }
        else
        { syslog( LOG_ERR, "argv[%d] = <null>", i ); }
    }

    closelog( );
}