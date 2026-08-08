/* GordOS config.h for TCC, hand written since TCC's configure script has no
    concept of GordOS as a target OS.
    
    The four path macros (CRTPREFIX, LIBPATHS, SYSINCLUDEPATHS, ELFINTERP) arer
    deliberately NOT defined here, tcc.h wraps each in an #ifndef guard, and they
    get supplied ad -D flags in the build recipe instead (mirroring how upstream's Makefile
    does it). That means zero patches needed to tcc.h's Linux-spesific fallback code,
    our -D values simply take priority via the same guards. */

#define TCC_VERSION "0.9.28rc-gordos"
#define TCC_TARGET_I386 1
#define TCC_TARGET_GORDOS 1

#define CONFIG_TCCDIR "/lib"
#define CONFIG_TCC_PREDEFS 1

/*GordOS has no dynamic linker at all, static linking isnt a mode you opt into, its the
    only mode. Rather than patching static_link directly in libtcc.c, this rides tcc_new()'s
    existing CONFIG_TCC_SWITCHES mechanism (an existing extension point already used
    upstream for platform specific default flags) to apply -static automatically,
    exactly as if every invocation passwd it. */
#define CONFIG_TCC_SWITCHES "-static"

/* GordOS has no threads and no POSIX semaphores, this disables
   TCC's optional multi-threaded-compilation locking entirely, which
   otherwise pulls in <semaphore.h>/<dispatch/dispatch.h> and real
   sem_init/sem_wait we have no equivalent for. */
#define CONFIG_TCC_SEMLOCK 0