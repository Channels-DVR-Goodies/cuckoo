/**
 * @file cuckoo.c
 *
 * requires libconfig
 *
 * Created by Paul Chambers on 5/3/21.
 * MIT Licensed
 */

#define _GNU_SOURCE            1

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
#include <sys/wait.h>
#include <fcntl.h>
#include <ftw.h>

typedef struct {
    char * path;
    char * directory;
    char * name;
} tSplitPath;

const char * usageInstructions =
{
    "\n"
    "Usage: cuckoo <pathname>"
    "  Creates a subdirectory and moves the executable found at <pathname> into it.\n"
    "  A symlink is then created at <pathname> that points to this executable.\n"
    "  When this executable is invoked through the symlink, it goes through the\n"
    "  subdirectory in alphabetical order, executing every executable it finds\n"
    "  there, with the same command line parameters and environment it was\n"
    "  invoked with through the symlink. This allows multiple executables to\n"
    "  be executed transparently each time the original <pathname> executable\n"
    "  would have been invoked before these changes were made.\n"
    "\n"
    "More information can be found at https://paul-chambers.github.io/cuckoo\n"
};

void usage( const char * format, ... )
{
    va_list args;
    va_start( args, format );

    vfprintf( stderr, format, args );
    fprintf( stderr, "%sn", usageInstructions );

    va_end( args );
}

/**
 * @brief split path into directory and name
 * @param path
 */
void splitPath( tSplitPath * path )
{
    struct stat pathInfo;

    stat( path->path, &pathInfo );
    if ( S_ISDIR(pathInfo.st_mode ) )
    {
        path->directory = path->path;
        path->name = strdup( "" );
    }
    else
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
            path->name = strdup(lastSlash + 1);
            path->directory = realloc( path->directory, (lastSlash - path->directory) + 1 );
        }
    }
}

/**
 * @brief merge directory and name to create a path
 * @param ->path filled in with absolute path
 */
void mergePath( tSplitPath * path )
{
    if ( path != NULL )
    {
        /* if there's already a value, free it */
        if ( path->path != NULL )
        {
            free( path->path );
            path->path = NULL;
        }

        /* fix up the directory component, so it's an absolute path */
        if ( path->directory == NULL )
        {
            /* no directory component, so use the current working directory */
            path->directory = getcwd( NULL, 0 );
        }
        else if ( path->directory[0] != '/' )
        {
            /* existing relative path, so convert to absolute */
            char * dir = path->directory;
            path->directory = realpath( dir, NULL );
            free( dir );
        }

        /* make sure that snprintf doesn't emit '(null)' */
        if ( path->name == NULL )
        {
            path->name = strdup( "" );
        }

        char temp[PATH_MAX];
        snprintf( temp, sizeof(temp), "%s/%s", path->directory, path->name );
        path->path = realpath( temp, NULL );
    }
}

void freeSplitPath( tSplitPath ** pPath )
{
    tSplitPath * path = *pPath;
    if ( path != NULL )
    {
        if (path->path != NULL)
        {
            free( path->path );
            path->path = NULL;
        }
        if (path->directory != NULL)
        {
            free( path->directory );
            path->directory = NULL;
        }
        if (path->name != NULL)
        {
            free( path->name );
            path->name = NULL;
        }
        free( path );
        *pPath = NULL;
    }
}

tSplitPath * getArgv0Path(const char * argv0 )
{
    tSplitPath * myPath = calloc( 1, sizeof(tSplitPath) );

    if (myPath != NULL)
    {
        myPath->path = strdup( argv0 );
        splitPath( myPath );

        char * p = myPath->directory;
        myPath->directory = realpath( p, NULL );
        free( p );

        free( myPath->path );
        size_t size = strlen( myPath->directory ) + strlen( myPath->name) + sizeof("/");
        myPath->path = malloc( size );
        if ( myPath->path != NULL )
        {
            snprintf( myPath->path, size, "%s/%s", myPath->directory, myPath->name );
        }
    }

    return myPath;
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
    char temp[PATH_MAX + 1];
    temp[0] = '\0';
    int len = readlink( "/proc/self/exe", temp, sizeof(temp));
    if ( len >= 0 )
    {
        temp[len] = '\0';
    }
    return strdup( temp );
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

                const char * execPath = getPathToSelf();

                /* create a symlink to ourselves as the executable we just moved */
                if ( symlink( execPath, installPath->path ) != 0 )
                {
                    fprintf( stderr, "err: unable to symlink \'%s\' to \'%s\' (%d: %s)\n",
                             installPath->path, execPath, errno, strerror(errno));
                    return errno;
                }
                else
                {
                    fprintf( stderr, "Installed \'%s\' to \'%s\' successfully.\n"
                                     "The script directory can be found at \'%s\'\n",
                             execPath, installPath->path, scriptsPath->directory );
                }
                free( (void *)execPath );
            }
            freeSplitPath( &scriptsPath );
        }

        freeSplitPath( &installPath );
    }

    /* we probably should free installPath, scriptsPath and execPath,
     * but it's a little complicated, and we're about to quit anyhow */

    return result;
}

/**
 * @brief launches an executable
 * @param argv array of arguments. argv[0] is path to executable. terminated by null pointer.
 * @param envp array of environment values, terminated by null pointer.
 * @return exit code from launched process.
 */
int launch( char * argv[], char * envp[] )
{
    int result = 0;
    int status;

    /* `vfork()` is slightly more efficient than `fork()` for this common scenario, where the child
     * immediately calls one of the variants of `execve()`, as it doesn't bother creating a copy of
     * the page tables, only to blow them away immediately by calling `execve()` */
    int pid = vfork();
    switch ( pid )
    {
    case -1: /* fork failed */
        syslog( LOG_ERR, "err: unable to launch \'%s\' (%d: %s)", argv[0], errno, strerror(errno) );
        result = errno;
        break;

    case 0: /* this execution thread is the child */
        execve( argv[0], argv, envp );
        break; /* execve should never return... */

    default: /* this execution thread is the parent - 'pid' is of the child */
        waitpid( pid, &status, 0 );
        if ( WEXITSTATUS( status ) )
        {
            result = WEXITSTATUS( status );
        }
        break;
    }
    return result;
}

typedef struct sExecutable {
    struct sExecutable * next;
    char                 path[1];
} tExecutable;

tExecutable * executableHead;

int forEachEntry( const char * path, const struct stat *info, int entryType, struct FTW * ftw )
{
    int result = FTW_CONTINUE;

    // fprintf( stderr, "level: %d, path: %s\n",  ftw->level, path );
    switch (entryType)
    {
    case FTW_D:
    case FTW_DP:
    case FTW_DNR:
        if ( ftw->level > 0 )
        {
            /* for any directory apart from the root one, don't decend into it */
            result = FTW_SKIP_SUBTREE;
        }
        break;

    case FTW_F:
        /* we were given a file - is it executable? */
        if ( faccessat( AT_FDCWD, path, X_OK, 0 ) == 0 )
        {
            int pathLen = strlen( path );
            tExecutable * executable = calloc( 1, sizeof( tExecutable ) + pathLen );
            if ( executable != NULL )
            {
                memcpy( executable->path, path, pathLen );
                tExecutable ** prev = &executableHead;
                tExecutable * exct = executableHead;
                while ( exct != NULL )
                {
                    if ( strcoll(executable->path, exct->path) < 0 )
                    {
                        /* insewrt the new entry before this one */
                        break;
                    }
                    prev = &exct->next;
                    exct = exct->next;
                }
                executable->next = exct;
                *prev = executable;
            }
        }
        break;

    default:
        break;
    }

    return result;
}

int invoke( tSplitPath * myPath, char * argv[], char * envp[] )
{
    int result = 0;

    tSplitPath *scriptsPath = getScriptsPath( myPath );
    // fprintf( stderr, "directory %s\n", scriptsPath->directory );

    executableHead = NULL;

    nftw( scriptsPath->directory, forEachEntry, 2, FTW_ACTIONRETVAL );

    tExecutable * executable = executableHead;
    while ( executable != NULL )
    {
        argv[0] = executable->path;
        int res = launch( argv, envp );
        if ( result == 0 && res != 0 )
        {
            result = res;
        }

        /* done with this one, so free it */
        tExecutable * f = executable;
        executable = executable->next;
        free( f );
    }

    executableHead = NULL;

    freeSplitPath( &scriptsPath );

    return result;
}


int main( int argc, char * argv[], char * envp[] )
{
    int result = 0;

    FILE * output = stdout;

    tSplitPath * myPath = getArgv0Path( argv[0] );

    if ( myPath != NULL)
    {
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
            result = invoke( myPath, argv, envp );
        }

        freeSplitPath( &myPath );

        closelog();
    }

    return result;
}
