/*
 * Copyright (c) 2015, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by Adam Moody <moody20@llnl.gov>.
 * LLNL-CODE-667277.
 * All rights reserved.
 * This file is part of the SpawnNet library.
 * For details, see https://github.com/hpc/spawnnet
 * Please also read the LICENSE file.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "spawn_internal.h"

static char  my_prog[] = "mpispawn";
static char* my_host = NULL;
static pid_t my_pid;

/* initialize our hostname and pid if we don't have it */
static void get_name()
{
  if (my_host == NULL) {
    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
      my_host = strdup(hostname);
    } else {
      my_host = strdup("NULLHOST");
    }
    my_pid = getpid();
  }
  return;
}

/* print error message */
void spawn_dbg(const char* file, int line, const char* format, ...)
{
  get_name();

  va_list args;
  char* str = NULL;

  /* check that we have a format string */
  if (format == NULL) {
    return;
  }

  /* compute the size of the string we need to allocate */
  va_start(args, format);
  int size = vsnprintf(NULL, 0, format, args) + 1;
  va_end(args);

  /* allocate and print the string */
  if (size > 0) {
    /* NOTE: we don't use spawn_malloc to avoid infinite loop */
    str = (char*) malloc(size);
    if (str == NULL) {
      /* error */
      return;
    }

    /* format message */
    va_start(args, format);
    vsnprintf(str, size, format, args);
    va_end(args);

    /* grab timestamp */
    char time_str[30];
    time_t timestamp = time(NULL);
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", localtime(&timestamp));

    /* print message */
    fprintf(stderr, "DEBUG: %s on %s:%d at %s: %s @ %s:%d\n",
        my_prog, my_host, my_pid, time_str, str, file, line
    );
    fflush(stderr);

    free(str);
  }

  return;
}

/* print error message */
void spawn_err(const char* file, int line, const char* format, ...)
{
  get_name();

  va_list args;
  char* str = NULL;

  /* check that we have a format string */
  if (format == NULL) {
    return;
  }

  /* compute the size of the string we need to allocate */
  va_start(args, format);
  int size = vsnprintf(NULL, 0, format, args) + 1;
  va_end(args);

  /* allocate and print the string */
  if (size > 0) {
    /* NOTE: we don't use spawn_malloc to avoid infinite loop */
    str = (char*) malloc(size);
    if (str == NULL) {
      /* error */
      return;
    }

    /* format message */
    va_start(args, format);
    vsnprintf(str, size, format, args);
    va_end(args);

    /* grab timestamp */
    char time_str[30];
    time_t timestamp = time(NULL);
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", localtime(&timestamp));

    /* print message */
    fprintf(stderr, "ERROR: %s on %s:%d at %s: %s @ %s:%d\n",
        my_prog, my_host, my_pid, time_str, str, file, line
    );
    fflush(stderr);

    free(str);
  }

  return;
}

/* wrapper to exit (useful to place debugger breakpoints) */
void spawn_exit(int code)
{
  /* TODO: may want to print message and sleep before exitting,
   * capturing under a debugger */
  exit(code);
}

/* allocate size bytes, returns NULL if size == 0,
 * fatal error if allocation fails */
void* spawn_malloc(size_t size, const char* file, int line)
{
  void* ptr = NULL;
  if (size > 0) {
    ptr = malloc(size);
    if (ptr == NULL) {
      /* error */
      spawn_err(file, line, "Failed to allocate %llu bytes", (unsigned long long) size);
      spawn_exit(1);
    }
  }
  return ptr;
}

char* spawn_strdup(const char* file, int line, const char* origstr)
{
  char* str = NULL;
  if (origstr != NULL) {
    str = strdup(origstr);
    if (str == NULL) {
      /* error */
      spawn_err(file, line, "Failed to allocate string (strdup() errno=%d %s)", errno, strerror(errno));
      spawn_exit(1);
    }
  }
  return str;
}

char* spawn_strdupf(const char* file, int line, const char* format, ...)
{
  va_list args;
  char* str = NULL;

  /* check that we have a format string */
  if (format == NULL) {
    return NULL;
  }

  /* compute the size of the string we need to allocate */
  va_start(args, format);
  int size = vsnprintf(NULL, 0, format, args) + 1;
  va_end(args);

  /* allocate and print the string */
  if (size > 0) {
    str = (char*) spawn_malloc(size, file, line);

    va_start(args, format);
    vsnprintf(str, size, format, args);
    va_end(args);
  }

  return str;
}

/* free memory and set pointer to NULL */
void spawn_free(void* arg_pptr)
{
  void** pptr = (void**) arg_pptr;
  if (pptr != NULL) {
    /* get pointer to memory and call free if it's not NULL*/
    void* ptr = *pptr;
    if (ptr != NULL) {
      free(ptr);
    }

    /* set caller's pointer to NULL */
    *pptr = NULL;
  }
  return;
}

size_t spawn_pack_uint64(void* buf, uint64_t val)
{
  uint64_t val_net = spawn_hton64(val);
  memcpy(buf, &val_net, 8);
  return 8;
}

size_t spawn_unpack_uint64(const void* buf, uint64_t* val)
{
  uint64_t val_net;
  memcpy(&val_net, buf, 8);
  *val = spawn_ntoh64(val_net);
  return 8;
}
