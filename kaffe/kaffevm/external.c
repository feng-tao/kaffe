/*
 * external.c
 * Handle method calls to other languages.
 *
 * Copyright (c) 1996, 1997, 1998, 1999
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * Copyright (c) 2004
 *      Kaffe.org contributors. See ChangeLogs for details. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#if defined(HAVE_STDARG_H)
#include <stdarg.h>
#endif /* defined(HAVE_STDARG_H) */

#include <stdio.h>

#include "config.h"
#include "debug.h"
#include "config-std.h"
#include "config-mem.h"
#include "config-io.h"
#include "gtypes.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "slots.h"
#include "external.h"
#include "errors.h"
#include "exception.h"
#include "slib.h"
#include "system.h"
#include "support.h"
#include "md.h"
#include "system.h"
#include "jthread.h"
#include "jsignal.h"
#include "stats.h"
#include "native-wrapper.h"
#define LT_NON_POSIX_NAMESPACE
#include "ltdl.h"
#if defined(KAFFE_FEEDBACK)
#include "feedback.h"
#endif
#include "jni_i.h"
#ifdef ENABLE_BINRELOC
#include "prefix.h"
#endif

#ifdef __riscos__
#include <unixlib/local.h>
#endif

#ifndef STUB_PREFIX
#define STUB_PREFIX ""
#define STUB_PREFIX_LEN 0
#endif

#ifndef STUB_POSTFIX
#define STUB_POSTFIX ""
#endif

#ifndef LIBRARYHANDLE
#define LIBRARYHANDLE lt_dlhandle
#endif

#ifndef LIBRARYINIT
#define LIBRARYINIT() (lt_dlinit())
#endif

#ifndef LIBRARYSUFFIX
#define LIBRARYSUFFIX ""
#endif

#ifndef LIBRARYLOAD
#define LIBRARYLOAD(desc,filename) ((desc)=lt_dlopenext((filename)))
#endif

#ifndef LIBRARYUNLOAD
#define LIBRARYUNLOAD(desc)		(lt_dlclose(desc))
#endif

#ifndef LIBRARYERROR
#define LIBRARYERROR() lt_dlerror()
#endif

static struct _libHandle {
	LIBRARYHANDLE	desc;
	char*		name;
	int		ref;
} libHandle[MAXLIBS];

#ifndef LIBRARYFUNCTION
static inline lt_ptr_t findLibraryFunction(const char *name) {
  int i = 0;
  lt_ptr_t ptr = 0;

  while (!ptr && libHandle[i].ref && i < MAXLIBS) {
    ptr = lt_dlsym(libHandle[i].desc, name);

DBG(NATIVELIB,
    if (ptr == NULL) {
	dprintf("Couldn't find %s in library handle %d == %s.\nError message is %s.\n",
		name,
		i,
		lt_dlgetinfo(libHandle[i].desc) == NULL ? "unknown" : lt_dlgetinfo(libHandle[i].desc)->name,
		lt_dlerror());
    }
    else {
	dprintf("Found %s in library handle %d == %s.\n",
		name,
        	i,
        	lt_dlgetinfo(libHandle[i].desc) == NULL ? "unknown" : lt_dlgetinfo(libHandle[i].desc)->name);
    }
)

    ++i;
  }

  return ptr;
}

#define LIBRARYFUNCTION(ptr,name) ((ptr)=findLibraryFunction(name))
#endif

static char *libraryPath = "";

extern JavaVM Kaffe_JavaVM;

/*
 * Error stub function.  Point unresolved link errors here to avoid
 * problems.
 */
static
void*
error_stub(void)
{
	return (0);
}

#ifdef ENABLE_BINRELOC
static
const char *discoverLibraryHome()
{
  static char libraryHome[MAXPATHLEN];
  char *entryPoint;

  strcpy(libraryHome, SELFPATH);
  
  if (strlen(file_separator) != 1)
    {
      fprintf(stderr, "WARNING: Impossible to auto-discover the home of native libraries\n");
      return NULL;
    }

  entryPoint = strrchr(libraryHome, file_separator[0]);
  if (entryPoint == NULL)
    // Abnormal. We may return ".".
    return ".";

  *entryPoint = 0;
  
  return libraryHome;
}
#endif

void
initNative(void)
{
	char lib[MAXLIBPATH];
	char* lpath;
	char* nptr;
	char* ptr;
	unsigned int len;

	DBG(INIT, dprintf("initNative()\n"); )

	lpath = (char*)Kaffe_JavaVMArgs.libraryhome;
	if (lpath == 0) {
		lpath = getenv(LIBRARYPATH);
#ifdef ENABLE_BINRELOC
		if (lpath == 0) {
			lpath = discoverLibraryHome();
		}
#endif
	}
#ifdef __riscos__
        __unixify(lpath, 0, lib, MAXLIBPATH, __RISCOSIFY_FILETYPE_NOTSPECIFIED);
        lpath = strdup(lib);
#endif

	len = 0;
	if (lpath != 0) {
		len += strlen(lpath);
	}

#ifdef JNI_LIBRARY_PATH
	len += strlen (path_separator);
	len += strlen (JNI_LIBRARY_PATH);
#endif

	/*
	 * Build a library path from the given library path.
	 */
	libraryPath = gc_malloc(len+1, KGC_ALLOC_NATIVELIB);
	addToCounter(&ltmem, "vmmem-libltdl", 1, GCSIZEOF(libraryPath));
	if (lpath != 0) {
		strcat(libraryPath, lpath);
	}

#ifdef JNI_LIBRARY_PATH
	strcat (libraryPath, path_separator);
	strcat (libraryPath, JNI_LIBRARY_PATH);
#endif

	DBG(INIT, dprintf("got lpath %s and libraryPath %s\n", lpath, libraryPath); )

	LIBRARYINIT();

	/* Find the default library */
	for (ptr = libraryPath; ptr != 0; ptr = nptr) {
		nptr = strstr(ptr, path_separator);
		if (nptr == 0) {
			strcpy(lib, ptr);
		}
		else if (nptr == ptr) {
			nptr += strlen(path_separator);
			continue;
		}
		else {
			strncpy(lib, ptr, (size_t)(nptr - ptr));
			lib[nptr-ptr] = '\0';
			nptr += strlen(path_separator);
		}
		strcat(lib, "/"); /* should be file_separator, libltdl
				     does not handle backslashes yet */
		strcat(lib, NATIVELIBRARY);
		strcat(lib, LIBRARYSUFFIX);

		if (loadNativeLibrary(lib, NULL, 0) >= 0) {
			DBG(INIT, dprintf("initNative() done\n"); )
			return;
		}
	}
	dprintf("Failed to locate native library \"%s\" in path:\n", NATIVELIBRARY);
	dprintf("\t%s\n", libraryPath);
	dprintf("Aborting.\n");
	fflush(stderr);
	EXIT(1);
}

int
loadNativeLibrary(char* lib, char *errbuf, size_t errsiz)
{
	int retval;

	retval = loadNativeLibrary2(lib, 1, errbuf, errsiz);
	return( retval );
}

/*
 * Link in a native library. If successful, returns an index >= 0 that
 * can be passed to unloadNativeLibrary(). Otherwise, returns -1 and
 * fills errbuf (if not NULL) with the error message. Assumes synchronization.
 */
int
loadNativeLibrary2(char* path, int default_refs, char *errbuf, size_t errsiz)
{
	struct _libHandle *lib;
	int libIndex;
	void *func;

	/* Find a library handle.  If we find the library has already
	 * been loaded, don't bother to get it again, just increase the
	 * reference count.
	 */
	for (libIndex = 0; libIndex < MAXLIBS; libIndex++) {
		lib = &libHandle[libIndex];
		if (lib->desc == 0) {
			goto open;
		}
		if (strcmp(lib->name, path) == 0) {
			lib->ref++;
DBG(NATIVELIB,
			dprintf("Native lib %s\n"
			    "\tLOAD desc=%p index=%d ++ref=%d\n",
			    lib->name, lib->desc, libIndex, lib->ref);
    )
			return libIndex;
		}
	}
	if (errbuf != 0) {
		assert(errsiz > 0);
		strncpy(errbuf, "Too many open libraries", errsiz);
		errbuf[errsiz - 1] = '\0';
	}
	return -1;

	/* Open the library */
	open:

#if 0
	/* If this file doesn't exist, ignore it */
	if (access(path, R_OK) != 0) {
		if (errbuf != 0) {
			assert(errsiz > 0);
			strncpy(errbuf, SYS_ERROR(errno), errsiz);
			errbuf[errsiz - 1] = '\0';
		}
		return -1;
	}
#endif
/* if we tested for existence here, libltdl wouldn't be able to look
   for system-dependent library names */

	blockAsyncSignals();
	{
                LIBRARYLOAD(lib->desc, path);
                if (lib->desc == 0)	
			{
				const char *err = LIBRARYERROR();
				
				/* XXX Bleh, silly guessing system. */
				if( err == 0 )
				{
					if (errbuf != 0)
						strncpy(errbuf,
							"Unknown error",
							errsiz);
				}
				else if( (strstr(err, "ile not found") ||
					  strstr(err, "annot open")) )
				{
                                char *last_sep = strrchr (path, file_separator[0]);
#ifdef HAVE_SNPRINTF
                                snprintf (errbuf, errsiz, "%s: not found.\n%s",
                                          last_sep==NULL?path:last_sep+1,
					  err);
#else
                                /* possible buffer overflow problem */
                                if(errbuf != 0)
                                    sprintf (errbuf, "%s: not found.\n%s",
                                          last_sep==NULL?path:last_sep+1,
					  err);
#endif
				}
				else
				{
					/* We'll assume its a real error. */
					if (errbuf != 0)
						strncpy(errbuf, err, errsiz);
				}
			}
		}
	unblockAsyncSignals();

	if (lib->desc == 0) {
		return -1;
	}

        lib->name = gc_malloc(strlen(path)+1, KGC_ALLOC_NATIVELIB);
        strcpy (lib->name, path);

	lib->ref = default_refs;
	addToCounter(&ltmem, "vmmem-libltdl", 1, GCSIZEOF(lib->name));

DBG(NATIVELIB,
	dprintf("Native lib %s\n"
	    "\tLOAD desc=%p index=%d ++ref=%d\n",
	    lib->name, lib->desc, libIndex, lib->ref);
    )
#if defined(KAFFE_FEEDBACK)
	feedbackLibrary(path, true);
#endif
 
        func = loadNativeLibrarySym("JNI_OnLoad"); 
	if (func != NULL) {
	    JavaVM *jvm = &Kaffe_JavaVM;
	    /* Call JNI_OnLoad */
	    ((jint(JNICALL *)(JavaVM *, void *))func)(jvm, NULL);
	}

	return libIndex;
}

/*
 * Unlink a native library. Assumes synchronization.
 * Note that libnative is always at index zero and should
 * never be unloaded. So index should never equal zero here.
 */
void
unloadNativeLibrary(int libIndex)
{
	struct _libHandle *lib;

	assert(libIndex > 0 && libIndex < MAXLIBS);
	lib = &libHandle[libIndex];

DBG(NATIVELIB,
	dprintf("Native lib %s\n"
	    "\tUNLOAD desc=%p index=%d --ref=%d\n",
	    lib->name, lib->desc, libIndex, lib->ref - 1);
    )

	assert(lib->desc != 0);
	assert(lib->ref > 0);
	if (--lib->ref == 0) {
		blockAsyncSignals();
		LIBRARYUNLOAD(lib->desc);
		unblockAsyncSignals();
		KFREE(lib->name);
		lib->desc = 0;
	}
}

/*
 * Get pointer to symbol from symbol name.
 */
void*
loadNativeLibrarySym(const char* name)
{
	void* func;

	blockAsyncSignals();
	LIBRARYFUNCTION(func, name);
	unblockAsyncSignals();

	return (func);
}


static void
strcatJNI(char* to, const char* from)
{
	char* ptr;

	ptr = &to[strlen(to)];
	for (; *from != 0; from++) {
		switch (*from) {
		case '(':
			/* Ignore */
			break;
		case ')':
			/* Terminate here */
			goto end;
		case '_':
			*ptr++ = '_';
			*ptr++ = '1';
			break;
		case ';':
			*ptr++ = '_';
			*ptr++ = '2';
			break;
		case '[':
			*ptr++ = '_';
			*ptr++ = '3';
			break;
		case '/':
			*ptr++ = '_';
			break;
		default:
			*ptr++ = *from;
			break;
		}
	}

	end:;
	*ptr = 0;
}




/*
 * Look up a native function using the JNI interface system.
 */
static jint
Kaffe_JNI_native(Method* meth)
{
	char name[1024];
	void* func;

	/* Build the simple JNI name for the method */
#if defined(NO_SHARED_LIBRARIES)
        strcpy(name, "Java_");
#elif defined(HAVE_DYN_UNDERSCORE)
	strcpy(name, "_Java_");
#else
	strcpy(name, "Java_");
#endif
	strcatJNI(name, meth->class->name->data);
	strcat(name, "_");
	strcatJNI(name, meth->name->data);

	func = loadNativeLibrarySym(name);
	if (func == NULL) {
		/* Try the long signatures */
		strcat(name, "__");
		strcatJNI(name, METHOD_SIGD(meth));
		func = loadNativeLibrarySym(name);
		if (func == 0) {
			return (JNI_FALSE);
		}
	}

	meth->accflags |= ACC_JNI;

	/* Wrap the function in a calling wrapper */
	engine_create_wrapper(meth, func);

	return (JNI_TRUE);
}


bool
native(Method* m, errorInfo *einfo)
{
	char stub[MAXSTUBLEN];
	const char* s;
	int i;
	void* func;

	/* Construct the stub name */
	strcpy(stub, STUB_PREFIX);
	s = m->class->name->data;
	for (i = STUB_PREFIX_LEN; *s != 0; s++, i++) {
		if (*s == '/') {
			stub[i] = '_';
		}
		else {
			stub[i] = *s;
		}
	}
	stub[i] = '_';
	stub[i+1] = 0;
	strcat(stub, m->name->data);
	strcat(stub, STUB_POSTFIX);

DBG(NATIVELIB,	
	dprintf("Method = %s.%s%s\n", m->class->name->data, 
		m->name->data, METHOD_SIGD(m));
	dprintf("Native stub = '%s'\n", stub);
    )

	/* Find the native method */
	func = loadNativeLibrarySym(stub);
	if (func != 0) {
		/* Fill it in */
		engine_create_wrapper(m, func);
		return (true);
	}

	/* Try to locate the nature function using the JNI interface */
        if (Kaffe_JNI_native(m)) {
                return (true);
        }

DBG(NATIVELIB,
	dprintf("Failed to locate native function:\n\t%s.%s%s\n",
		m->class->name->data, m->name->data, METHOD_SIGD(m));
    )
#if defined(TRANSLATOR)
	{
		/* Work around for KFREE() ? : bug in gcc 2.7.2 */
		void *nc = METHOD_NATIVECODE(m);
		KFREE(nc);
	}
#endif
	SET_METHOD_NATIVECODE(m, (void*)error_stub);

	postExceptionMessage(einfo, JAVA_LANG(UnsatisfiedLinkError),
		"Failed to locate native function:\t%s.%s%s",
		m->class->name->data, m->name->data, METHOD_SIGD(m));
	return (false);
}

/*
 * Return the library path.
 */
char*
getLibraryPath(void)
{
	return (libraryPath);
}
