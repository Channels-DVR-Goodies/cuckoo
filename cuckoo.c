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
#include <stdbool.h>

typedef struct {
    char * path;
    char * directory;
    char * name;
} tSplitPath;

void usage( const char * format, ... )
{
    va_list args;
    va_start( args, format );

    vfprintf( stderr, format, args );

    va_end( args );
}

void splitPath( tSplitPath * path )
{
    /* we're going to split it, so copy it first */
    path->directory = strdup( path->path );
    path->name = NULL;

    char * lastSlash = NULL;
    for ( char * p = path->directory; *p != '\0'; p++ )
    {
        if ( *p == '/' )
        {
            lastSlash = p;
        }
    }
    if ( lastSlash != NULL )
    {
        *lastSlash = '\0';
        path->name = lastSlash + 1;
    }
}

tSplitPath * getMyPath( const char * argv0 )
{
    tSplitPath * myPath = calloc( 1, sizeof(tSplitPath) );

    if (myPath != NULL)
    {
        char * argv0copy = strdup( argv0 );
        myPath->directory = realpath( dirname( argv0copy ), NULL);
        free( argv0copy );

        argv0copy = strdup( argv0 );
        myPath->name = strdup( basename( argv0copy ) );
        free( argv0copy );

        size_t size = strlen( myPath->directory ) + strlen( myPath->name ) + sizeof("/");
        myPath->path = malloc( size );
        if ( myPath->path != NULL )
        {
            snprintf( myPath->path, size, "%s/%s", myPath->directory, myPath->name );
        }
    }
}


tSplitPath * getInstallPath( const char * relAppPath )
{
    tSplitPath * result = calloc(1, sizeof(tSplitPath));

    if ( result != NULL)
    {
        result->path = realpath( relAppPath, NULL );
        if ( result->path == NULL )
        {
            usage( "err: unable to access \'%s\' (%d: %s)\n", relAppPath, errno, strerror( errno ) );
        }
        else
        {
            splitPath( result );

            if ( access( result->path, X_OK ) != 0 )
            {
                usage( "err: \'%s\' is not executable (%d: %s)\n",
                       result->path, errno, strerror( errno ) );
                free( result->path);
                free( result );
                result = NULL;
            }
        }
    }

    return result;
}


tSplitPath * getScriptsPath( tSplitPath * installPath )
{
    tSplitPath *scriptsPath = calloc(1, sizeof(tSplitPath) );
    if (scriptsPath != NULL )
    {
        size_t scriptsDirSize = strlen( installPath->directory) + strlen( installPath->name ) + sizeof("/..d");
        scriptsPath->directory = malloc( scriptsDirSize );
        if (scriptsPath->directory != NULL )
        {
            snprintf(scriptsPath->directory, scriptsDirSize, "%s/.%s.d",
                     installPath->directory, installPath->name );

            size_t scriptsNameSize = strlen( installPath->name ) + sizeof("00-");
            scriptsPath->name = malloc( scriptsNameSize );
            if (scriptsPath->name != NULL)
            {
                snprintf( scriptsPath->name, scriptsNameSize, "00-%s", installPath->name );
                size_t scriptsPathSize = scriptsDirSize + scriptsNameSize + sizeof("/");
                scriptsPath->path = malloc(scriptsPathSize);
                if (scriptsPath->path != NULL)
                {
                    snprintf( scriptsPath->path, scriptsPathSize, "%s/%s",
                              scriptsPath->directory, scriptsPath->name);
                }
            }
        }
    }

    return scriptsPath;
}

int makeScriptsDir( tSplitPath * scriptsPath )
{
    struct stat dirStat;

    /* establish the script directory */
    if ( stat( scriptsPath->directory, &dirStat ) == 0 )
    {
        if ( (dirStat.st_mode & S_IFMT) != S_IFDIR )
        {
            /* something there, but it's not a directory */
            fprintf( stderr, "err: \"%s\" exists, but is not a directory\n", scriptsPath->directory );
            return ENOTDIR;
        }
    }
    else
    {
        if ( errno != ENOENT )
        {
            fprintf( stderr, "err: unable to get info about \'%s\' (%d: %s)\n",
                     scriptsPath->directory, errno, strerror( errno ) );
            return errno;
        }
        else
        {
            /* not there, so create it */
            if ( mkdir( scriptsPath->directory, S_IRWXU | S_IRWXG ) != 0 )
            {
                fprintf( stderr, "err: failed to create \'%s\' (%d: %s)\n",
                         scriptsPath->directory, errno, strerror( errno ) );
                return errno;
            }
        }
    }
    return 0;
}

const char * getPathToSelf( void )
{
    /* figure out the absolute path to this executable */
    char execPath[PATH_MAX + 1];
    execPath[0] = '\0';
    size_t len = readlink( "/proc/self/exe", execPath, sizeof(execPath));
    if ( len > 0 )
    {
        execPath[len] = '\0';
    }
    return strdup( execPath );
};

/**
 * @brief do the shuffle to move the original executable into the .d folder, and creating the symlink.
 * @param app the target app to hook
 * @return exit code
 */
int install( const char *relAppPath )
{
    int        result = 0;
    tSplitPath * installPath;

    installPath = getInstallPath( relAppPath );
    if ( installPath != NULL)
    {
        const char * execPath;
        tSplitPath *scriptsPath = getScriptsPath( installPath );

        if ( scriptsPath != NULL)
        {
            result = makeScriptsDir( scriptsPath );
            if ( result != 0 )
            {
                return result;
            }
            else
            {
                /* first move the executable into the scripts dir and rename to come first */
                if ( rename( installPath->path, scriptsPath->path ) != 0 )
                {
                    fprintf( stderr, "err: failed to move \'%s\' to \'%s\' (%d: %s)\n",
                             installPath->path, scriptsPath->path, errno, strerror(errno));
                    return errno;
                }

                execPath = getPathToSelf();

                /* create a symlink to ourselves as the executable we just moved */
                if ( symlink( execPath, installPath->path ) != 0 )
                {
                    fprintf( stderr, "err: unable to symlink \'%s\' to \'%s\' (%d: %s)\n",
                             installPath->path, execPath, errno, strerror(errno));
                    return errno;
                }
            }
        }
        fprintf( stderr, "Installed \'%s\' to \'%s\' successfully.\n"
                         "The script directory can be found at \'%s\'\n",
                 execPath, installPath->path, scriptsPath->directory );
    }

    /* we probably should free installPath, scriptsPath and execPath,
     * but it's a little complicated, and we're about to quit anyhow */

    return result;
}

int invoke( tSplitPath * myPath, int argc, char * argv[], char * envp[] )
{
    int result = 0;

    return result;
}


int main( int argc, char * argv[], char * envp[] )
{
    int result = 0;

    FILE * output = stdout;

    tSplitPath * myPath = getMyPath( argv[0] );

    openlog( myPath->name, LOG_PID, LOG_USER );


    if ( strcmp( myPath->name, "cuckoo" ) == 0 )
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
        result = invoke( myPath, argc, argv, envp );
    }

    closelog();

    return result;
}
