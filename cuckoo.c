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
    "Usage: cuckoo <pathname>\n"
    "  Creates a subdirectory and moves the executable found at <pathname> into it.\n"
    "  A symlink is then created at <pathname> that points to this executable.\n"
#if 0
    "  When this executable is invoked through the symlink, it goes through the\n"
    "  subdirectory in alphabetical order, executing every executable it finds\n"
    "  there, with the same command line parameters and environment it was\n"
    "  invoked with through the symlink. This allows multiple executables to\n"
    "  be executed transparently each time the original <pathname> executable\n"
    "  would have been invoked before these changes were made.\n"
#endif
    "\n"
    "More information can be found at https://paul-chambers.github.io/cuckoo\n"
};

/**
 * @brief
 * @param format
 * @param ...
 */
void usage( const char * format, ... )
{
    va_list args;
    va_start( args, format );

    vfprintf( stderr, format, args );
    fprintf( stderr, "%s", usageInstructions );

    va_end( args );
}

/**
 * @brief
 * @param path
 * @return
 */
const char * dirnamedup( const char * path )
{
    char * result = NULL;

    char * copy = strdup( path );
    char * lastSlash = strrchr( copy,'/' );
    if ( lastSlash != NULL)
    {
        lastSlash[1] = '\0';
        result = realpath( copy, NULL );
    }
    else
    {
        result = get_current_dir_name();
    }
    free( copy );

    return result;
}

/**
 * @brief
 * @param path
 * @return
 */
const char * basenamedup( const char * path )
{
    char * result = NULL;

    char * lastSlash = strrchr( path,'/' );
    if ( lastSlash != NULL)
    {
        result = strdup( lastSlash + 1 );
    }
    else
    {
        result = strdup( path );
    }

    return result;
}


/**
 * @brief
 * @param scriptsDir
 * @return
 */
int makeScriptsDir( const char * scriptsDir )
{
    struct stat dirStat;

    /* establish the script directory */
    if ( stat( scriptsDir, &dirStat ) == 0 )
    {
        if ( (dirStat.st_mode & S_IFMT) != S_IFDIR )
        {
            /* something there, but it's not a directory */
            fprintf( stderr, "err: \"%s\" exists, but is not a directory\n", scriptsDir );
            return ENOTDIR;
        }
    }
    else
    {
        if ( errno != ENOENT )
        {
            fprintf( stderr, "err: unable to get info about \'%s\' (%d: %s)\n",
                     scriptsDir, errno, strerror(errno ) );
            return errno;
        }
        else
        {
            /* not there, so create it */
            if ( mkdir( scriptsDir, S_IRWXU | S_IRWXG ) != 0 )
            {
                fprintf( stderr, "err: failed to create \'%s\' (%d: %s)\n",
                         scriptsDir, errno, strerror(errno ) );
                return errno;
            }
        }
    }
    return 0;
}

/**
 * @brief get the full path to ourselves
 * @return absolute path to the executable used to create this process (caller should free)
 */
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
 * @param app the target executable to hook
 * @return exit code
 */
int install( char *argv[] )
{
    int        result = 0;

    const char * installName = basenamedup( argv[1]);
    if ( installName != NULL)
    {
        const char * installDir = dirnamedup( argv[1] );
        if ( installDir != NULL)
        {
            char *installPath;
            char *scriptsDir;
            if ( asprintf( &installPath, "%s/%s", installDir, installName ) < 0 )
            {
                fprintf( stderr, "err: unable to build path to the install executable \'%s\' (%d: %s)\n",
                         installPath, errno, strerror(errno));
                result = errno;
            }
            else if ( asprintf( &scriptsDir, "%s/.%s.d", installDir, installName ) < 0 )
            {
                fprintf( stderr, "err: unable to build path to the scripts directory \'%s\' (%d: %s)\n",
                         scriptsDir, errno, strerror(errno));
                result = errno;
            }
            else
            {
                result = makeScriptsDir( scriptsDir );
                if ( result == 0 )
                {
                    char * scriptsDest;
                    if ( asprintf( &scriptsDest, "%s/00-%s", scriptsDir, installName ) < 0 )
                    {
                        fprintf( stderr, "err: unable to construct destination path for \'%s\' (%d: %s)\n",
                                 installPath, errno, strerror(errno));
                    }
                    else
                    {
                        /* first move the executable into the scripts dir and rename to come first */
                        if ( rename( installPath, scriptsDest ) != 0 )
                        {
                            fprintf( stderr, "err: failed to move \'%s\' to \'%s\' (%d: %s)\n",
                                     installPath, scriptsDir, errno, strerror(errno));
                            return errno;
                        }
                        else
                        {
                            /* create a symlink to ourselves as the executable we just moved */
                            const char * execPath = getPathToSelf();

                            if ( symlink( execPath, installPath ) != 0 )
                            {
                                fprintf( stderr, "err: unable to symlink \'%s\' to \'%s\' (%d: %s)\n",
                                         installPath, execPath, errno, strerror(errno));
                                return errno;
                            }
                            else
                            {
                                fprintf( stderr, "Installed \'%s\' to \'%s\' successfully.\n"
                                                 "The script directory can be found at \'%s\'\n",
                                         execPath, installPath, scriptsDir );
                            }
                            free( (void *)execPath );
                        }
                        free( scriptsDest );
                    }
                }
                free( scriptsDir );
                free( installPath );
            }
            free( (void *)installDir );
        }
        free( (void *)installName );
    }
    /* we probably should free installPath, scriptsPath and execPath,
     * but it's a little complicated, and we're about to quit anyhow */

    return result;
}

/**
 * @brief launches an executable.
 * @param argv array of arguments. argv[0] is path to executable. terminated by null pointer.
 * @param envp array of environment values, terminated by null pointer.
 * @return exit code from the launched process.
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
        if ( WIFEXITED( status ) )
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

/**
 * @brief
 * @param path
 * @param info
 * @param entryType
 * @param ftw
 * @return
 */
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
            /* for any directory apart from the topmost one, don't descend into it */
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

/**
 * @brief
 * @param argv
 * @param envp
 * @return
 */
int invoke( char * argv[], char * envp[] )
{
    int result = 0;

    fprintf( stderr, "    argv[0]: %s\n", argv[0] );

    const char * installName = basenamedup( argv[0]);
    const char * installDir = dirnamedup( argv[0] );
    if ( installName != NULL && installDir != NULL)
    {
        char * scriptsDir;
        if ( asprintf( &scriptsDir, "%s/.%s.d", installDir, installName ) < 0 )
        {
            fprintf( stderr, "err: unable to build path to the scripts directory \'%s\' (%d: %s)\n",
                     scriptsDir, errno, strerror( errno ) );
        }
        else if ( scriptsDir != NULL )
        {
            fprintf( stderr, "    argv[0]: %s\ninstallName: %s\n installDir: %s\n scriptsDir: %s\n",
                     argv[0], installName, installDir, scriptsDir );

            executableHead = NULL;

            nftw( scriptsDir, forEachEntry, 2, FTW_ACTIONRETVAL );

            tExecutable * executable = executableHead;
            while ( executable != NULL)
            {
                argv[0] = executable->path;
                fprintf( stderr, "launch %s\n", argv[0] );
                int res = launch( argv, envp );
                if ( result == 0 && res != 0 )
                {
                    result = res;
                }

                /* done with this one, so free it */
                tExecutable *f = executable;
                executable = executable->next;
                free( f );
            }

            executableHead = NULL;

            free( scriptsDir );
        }
        free( (void *)installDir );
        free( (void *)installName );
    }
    return result;
}

/**
 * @brief
 * @param argc
 * @param argv
 * @param envp
 * @return
 */
int main( int argc, char * argv[], char * envp[] )
{
    int result = 0;

    const char * myName = basenamedup( argv[0] );

    if ( myName != NULL)
    {
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
                result = install( argv );
            }
        }
        else
        {
            /* it's an invocation through a symlink */
            result = invoke( argv, envp );
        }

        closelog();

        free( (void *)myName );
    }

    return result;
}
