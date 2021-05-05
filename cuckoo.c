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
#include <stdarg.h>
#include <libgen.h>
#include <linux/limits.h>
#include <errno.h>
#include <sys/stat.h>

void usage( const char * format, ... )
{
    va_list args;
    va_start( args, format );

    vfprintf( stderr, format, args );

    va_end( args );
}

/**
 * @brief do the shuffle to move the original executable into the .d folder, and creating the symlink.
 * @param app the target app to hook
 * @return exit code
 */
int install( const char * relAppPath )
{
    int result = 0;

    char * installPath = realpath( relAppPath, NULL );
    if ( installPath == NULL )
    {
        usage( "err: unable to access \'%s\' (%d: %s)\n", relAppPath, errno, strerror( errno ) );
    }
    else
    {
        struct stat dirStat;

        if ( access( installPath, X_OK ) != 0 )
        {
            usage( "err: \'%s\' is not executable (%d: %s)",
                   installPath, errno, strerror( errno ) );
            return errno;
        }

        char * instDir = strdup( installPath );
        char * installDir = dirname( instDir );

        char * instName = strdup( installPath );
        char * installName = basename( instName );

        char destDir[PATH_MAX];
        snprintf( destDir, sizeof(destDir), "%s/.%s.d", installDir, installName );

        if ( stat( destDir, &dirStat ) == 0 )
        {
            if ( dirStat.st_mode & S_IFMT != S_IFDIR )
            {
                fprintf( stderr, "err: existing fs object %02x\n", dirStat.st_mode & S_IRWXG );
                return errno;
            }
        }
        else
        {
            if ( errno != ENOENT )
            {
                fprintf( stderr, "err: stat %s (%d: %s)\n",
                         destDir, errno, strerror( errno ) );
                return errno;
            }
            else
            {
                if ( mkdir( destDir, S_IRWXU | S_IRWXG ) != 0 )
                {
                    fprintf( stderr, "err: failed to create \'%s\' (%d: %s)\n",
                             destDir, errno, strerror( errno ) );
                    return errno;
                }
                else
                {
                    // fprintf( stderr, "%s created\n", destDir );
                }
            }
        }

        char destPath[PATH_MAX];
        snprintf( destPath, sizeof(destPath), "%s/00-%s", destDir, installName );
        free( instName );

        if ( rename( installPath, destPath ) != 0 )
        {
            fprintf( stderr, "err: rename failed (%d: %s)\n", errno, strerror(errno) );
            return errno;
        }

        char execPath[PATH_MAX + 1];
        execPath[0] = '\0';
        long len = readlink( "/proc/self/exe", execPath, sizeof(execPath) - 1);
        if ( len > 0 )
        {
            execPath[len] = '\0';
        }

        if ( symlink( execPath, installPath ) != 0 )
        {
            fprintf( stderr, "err: unable to symlink %s to %s (%d: %s)\n",
                     installPath, execPath, strerror( errno ) );
        }

        free( instDir );
        free( installPath );
    }

    return result;
}

int invoke( int argc, char * argv[], char * envp[] )
{
    int result = 0;

    char * myPath;

    /* dirname may modify string, so make a copy */
    char * argv0 = strdup( argv[0] );
    char * path0 = dirname( argv0 );
    myPath = realpath( path0, NULL );

    char * wd = getcwd( NULL, 0 );

    /* log debug info */
#if 1

    if ( wd != NULL )
    {
        syslog( LOG_INFO, "cwd: \"%s\"", wd );
    }

    if ( myPath != NULL )
    {
        syslog( LOG_INFO, "abs: \"%s\"", myPath );
    }

    for ( int i = 0; i < argc; i++ )
    {
        syslog( LOG_INFO, " %2d: \"%s\"", i, argv[i] );
    }
#endif

#if 0
    if ( envp != NULL )
    {
        for ( int i = 0; envp[i] != NULL; i++ )
        {
            fprintf(output, "%s\n", envp[i] );
        }
    }
#endif

    /* clean up */
    free( argv0 );
    free( wd );

    return result;
}

int main( int argc, char * argv[], char * envp[] )
{
    int result = 0;

    FILE * output = stdout;

    char * myName = strrchr( argv[0], '/') + 1;
    if ( myName - 1 == NULL )
    {
        myName = argv[0];
    }

    openlog( myName, LOG_PID, LOG_USER );


    if ( strcmp( myName, "cuckoo" ) == 0 )
    {
        /* it's an install */
        if ( argc != 2 || argv[1] == NULL || strlen( argv[1] ) < 1 )
        {
            usage("please provide the path to the executable to intercept");
        }
        else
        {
            result = install( argv[1] );
        }
    }
    else
    {
        /* it's an invocation through a symlink */
        result = invoke( argc, argv, envp );
    }

    closelog();

    return result;
}
