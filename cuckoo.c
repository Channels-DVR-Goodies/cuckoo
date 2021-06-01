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

#define debugf( ... ) DebugF_( __func__, __LINE__, __VA_ARGS__ )

static void DebugF_( const char * function, const int line, const char * format, ... )
{
	va_list args;

	va_start( args, format );

	vfprintf( stderr, format, args );
	fprintf(  stderr, " (%s() at line %d)\n", function, line );

	va_end( args );
}

#define reportError( ... ) ReportError_( __func__, __LINE__, __VA_ARGS__ )

static int ReportError_( const char * function, const int line, const char * format, ... )
{
    va_list args;

    int savedErrno = errno;

    va_start( args, format );

    fprintf(  stderr, "err: " );
    vfprintf( stderr, format, args );
    fprintf(  stderr, " (%s() at line %d)\n", function, line );

    va_end( args );

    return savedErrno;
}

#define reportErrno( ... ) ReportErrno_( __func__, __LINE__, __VA_ARGS__ )

static int ReportErrno_( const char * function, const int line, const char * format, ... )
{
	va_list args;

	int savedErrno = errno;

	va_start( args, format );

	fprintf( stderr, "err: " );
	vfprintf( stderr, format, args );
	fprintf( stderr, " (%d: %s) in %s() at line %d\n", savedErrno, strerror( savedErrno ), function, line );

	va_end( args );

	return savedErrno;
}

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
const char * absolutePath( const char * path )
{
    char * result = NULL;
    struct stat pathInfo;
    char * dir;
    char * directory;
    char * filename;

    if( lstat( path, &pathInfo ) != 0 )
    {
        reportErrno( "unable to get information about \'%s\'", path );
        return NULL;
    }

    switch ( pathInfo.st_mode & S_IFMT )
    {
    case S_IFLNK:
        {
            char * copy = strdup( path );
            if ( copy != NULL )
            {
                filename = strrchr( copy, '/' );
                if ( filename == NULL )
                {
                    /* no slash, just the filename */
                    dir = "./";
                    filename = copy;
                }
                else
                {
                    dir = copy;
                    *filename++ = '\0';
                }

                directory = realpath( dir, NULL);
                if ( directory != NULL )
                {
                    asprintf( &result, "%s/%s", directory, filename );
                    free( directory );
                }

                free( copy );
            }

        }
        break;

    case S_IFREG:
    case S_IFDIR:
        result = realpath( path, NULL );
        break;

    default:
        /* whatever it is, we don't support it */
        reportError( "\'%s\' isn't supported", path );
        result = NULL;
        break;
    }

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
 * @brief creates all directories of a path that are missing (like mkdir -p)
 * @param path
 * @return
 */
static int mkDirRecurse( const char * path )
{
    int result;
    struct stat   dirStat;

    if ( stat( path, &dirStat ) != 0 )
    {
        if (errno == ENOENT)
        {
            char * parent = strdup( path );
            result = mkDirRecurse( dirname( parent ) );
            free( parent );
            if (result == 0)
            {
                result = mkdir( path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
                if ( result != 0 )
                {
                    reportErrno( "unable to create directory \'%s\'", path );
                }
            }
            return result;
        }
        else
        {
            return reportErrno( "unable to get information about \'%s\'", path );
        }
    }
    return 0;
}

/**
 * @brief
 * @param path
 * @return
 */
const char * makeDirectory( const char * path )
{
    const char *  result = NULL;
    struct stat   dirStat;

    /* does the script directory already exist? */
    if ( stat( path, &dirStat ) == 0 )
    {
        if ( S_ISDIR(dirStat.st_mode ) )
        {
            result = path;
        }
        else
        {
            /* something there already, but it's not a directory */
            reportErrno( "\'%s\' exists, but is not a directory", path );
        }
    }
    else
    {
        /* stat() failed */
        if ( errno != ENOENT )
        {
            /* report anything other than the expected 'not found' */
            reportErrno( "unable to get info about \'%s\'", path );
        }
        else if ( mkDirRecurse( path ) == 0 )
        {
            result = path;
        }
    }
    return strdup( result );
}

/**
 * @brief
 * @param absPathFrom
 * @param absPathTo
 * @return
 */
const char * makeSymlink( const char * absPathFrom, const char * absPathTo )
{
    const char *  result = NULL;
    struct stat   dirStat;

    debugf("from: \'%s\' to: \'%s\'", absPathFrom, absPathTo );
    /* does the link to the script directory already exist? */
    if ( stat( absPathFrom, &dirStat ) == 0 )
    {
        if ( S_ISLNK( dirStat.st_mode ) )
        {
            /* a symlink exists, we're good. */
            result = absPathFrom;
        }
        else
        {
            /* something there already, but it's not a symbolic link */
            reportErrno( "\'%s\' exists, but is not a symbolic link", absPathFrom );
        }
    }
    else
    {
        /* stat() failed */
        if ( errno != ENOENT )
        {
            /* report anything other than the expected 'not found' */
            reportErrno( "unable to get info about \'%s\'", absPathFrom );
        }
        else if ( symlink( absPathFrom, absPathTo ) != 0 )
        {
            reportErrno( "failed to create a symbolic link from \'%s\' to \'%s\'", absPathFrom, absPathTo );
        }
        else
        {
            /* successfully created the symlink */
            result = absPathFrom;
        }
    }
    return result;
}

/**
 * @brief
 * @param scriptsDir
 * @return
 */
const char * getScriptsDir( const char * absPath )
{
    const char *  result = NULL;

    char * dir = strdup( absPath );
    if ( dir != NULL )
    {
        char * base = strrchr( dir, '/' );
        if ( base != NULL)
        {
            *base++ = '\0';

            char * scriptsDir = NULL;
            asprintf( &scriptsDir, "%s/.%s.d", dir, base );
            if ( scriptsDir != NULL )
            {
                result = makeDirectory( scriptsDir );
                free( scriptsDir );
            }
        }
        free( dir );
    }

    return result;
}

/**
 * @brief
 * @param scriptsDir
 * @return
 */
const char * getCommonDir( const char * absPath )
{
    const char *  result = NULL;

    char * dir = strdup( absPath );
    if ( dir != NULL )
    {
        char * base = strrchr( dir, '/' );
        if ( base != NULL)
        {
            *base++ = '\0';

            char * commonDir = NULL;
            asprintf( &commonDir, "/etc/cuckoo/%s", base );
            if ( commonDir != NULL )
            {
                result = makeDirectory( commonDir );
                free( commonDir );
            }
        }
        free( dir );
    }

    return result;
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
int install( char * argv[] )
{
    int result = 0;

    const char * installPath = absolutePath( argv[1] );
    if ( installPath != NULL)
    {
        const char * filename = basenamedup( installPath );
        if ( filename != NULL )
        {
            const char * scriptsDir = getScriptsDir( installPath );
            if ( scriptsDir != NULL)
            {
                struct stat targetStat;
                if ( lstat( installPath, &targetStat ) != 0 )
                {
                    result = reportErrno( "unable to get information on \'%s\'", installPath );
                }
                else switch ( targetStat.st_mode & S_IFMT )
                {
                default:
                    /* whatever it is, we can't support it */
                    reportError( "\'%s\' ism't a supported file type", installPath );
                    result = -1;
                    break;

                case S_IFLNK:
                    /* we've been here already? */
                    printf( "nothing to do - \'%s\' is already a symlink\n", installPath );
                    result = 0;
                    break;

                case S_IFREG:
                    /* ok, at least it's a regular file. Is it executable? */
                    if ( (targetStat.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0 )
                    {
                        reportError( "\'%s\' is not executable", installPath );
                        result = -1;
                    }
                    else
                    {
                        /* move the executable into the scripts dir and rename it so it sorts first */
                        char * targetPath;
                        asprintf( &targetPath, "%s/50-%s", scriptsDir, filename );
                        if (targetPath != NULL)
                        {
                            if ( rename( installPath, targetPath ) != 0 )
                            {
                                result = reportErrno( "failed to move \'%s\' to \'%s\'", installPath, scriptsDir );
                            }
                            else
                            {
                                /* create a symlink to ourselves to pretend to be the executable we just moved */
                                const char * execPath = getPathToSelf();
                                if ( execPath != NULL )
                                {
                                    if ( symlink( execPath, installPath ) != 0 )
                                    {
                                        result = reportErrno( "unable to symlink \'%s\' to \'%s\'", installPath, execPath );
                                    }
                                    else
                                    {
                                        printf( "Successfully Installed \'%s\' to \'%s\'.\n"
                                                "The script directory can be found at \'%s\'\n",
                                                execPath, installPath, scriptsDir );
                                    }
                                    free((void *)execPath );
                                }
                            }
                            free( targetPath );
                        }
                    }
                    break;
                }
                free((void *)scriptsDir );
            }
            free((void *)filename );
        }
        free((void *)installPath );
    }

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

    /* `vfork()` is slightly more efficient than `fork()` for this common scenario,
     * where the child immediately calls one of the variants of `execve()`, so it
     * doesn't need to create a copy of the page tables, only to blow them away
     * immediately by calling `execve()`
     */
    int pid = vfork();
    switch ( pid )
    {
    case -1: /* fork failed */
        syslog( LOG_ERR, "err: unable to launch \'%s\'", argv[0] );
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
    struct sExecutable *  next;
    unsigned short        nameOffset;
    char                  path[1];
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

    // reportErrno("level: %d, path: %s\n",  ftw->level, path );
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
                char * name = strrchr( path, '/' );
                if ( name != NULL )
                {
                    executable->nameOffset = name - path + 1;
                }
                tExecutable ** prev = &executableHead;
                tExecutable *  exct = executableHead;
                while ( exct != NULL )
                {
                    if ( strcoll( &executable->path[ executable->nameOffset ],
                                  &exct->path[ exct->nameOffset ] ) < 0 )
                    {
                        /* insert the new entry before this one */
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

    // debugf("    argv[0]: %s\n", argv[0] );

    const char * installPath = absolutePath( argv[0] );
    if ( installPath != NULL )
    {
        const char * scriptsDir = getScriptsDir( installPath );
        if ( scriptsDir != NULL )
        {
            const char * commonDir = getCommonDir( installPath );
            if (commonDir != NULL )
            {
#if 0
                debugf( "     argv[0]: %s\n"
                        " installPath: %s\n"
                        "  scriptsDir: %s\n"
                        "   commonDir: %s\n",
                        argv[0], installPath, scriptsDir, commonDir );
#endif
                executableHead = NULL;

                nftw( scriptsDir, forEachEntry, 2, FTW_ACTIONRETVAL );
                nftw( commonDir,  forEachEntry, 2, FTW_ACTIONRETVAL );

                tExecutable * executable = executableHead;
                while ( executable != NULL)
                {
                    argv[0] = executable->path;
                    // debugf( "launch %s", argv[0] );
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

                free( (void *)commonDir );
            }
            free( (void *)scriptsDir );
        }
        free( (void *)installPath );
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
