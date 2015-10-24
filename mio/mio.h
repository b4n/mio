/*
 *  MIO, an I/O abstraction layer replicating C file I/O API.
 *  Copyright (C) 2010  Colomban Wendling <ban@herbesfolles.org>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 */

#ifndef H_MIO_H
#define H_MIO_H

#include <stdio.h>
#include <stdarg.h>

#if ! (defined (__attribute__) || defined (__GNUC__))
# define __attribute__(x) /* nothing */
#endif


/**
 * MIOType:
 * @MIO_TYPE_FILE: #MIO object works on a file
 * @MIO_TYPE_MEMORY: #MIO object works in-memory
 * 
 * Existing implementations.
 */
enum _MIOType {
  MIO_TYPE_FILE,
  MIO_TYPE_MEMORY
};

typedef enum _MIOType   MIOType;
typedef struct _MIO     MIO;
typedef struct _MIOPos  MIOPos;
/**
 * MIOReallocFunc:
 * @ptr: Pointer to the memory to resize
 * @size: New size of the memory pointed by @ptr
 * 
 * A function following the realloc() semantic.
 * 
 * Returns: A pointer to the start of the new memory, or %NULL on failure.
 */
typedef void  *(* MIOReallocFunc) (void    *ptr,
                                   size_t   size);
/**
 * MIOFreeFunc:
 * @ptr: Pointer to the memory to free
 * 
 * A function following the free() semantic.
 */
typedef void   (* MIOFreeFunc)    (void *ptr);

/**
 * MIOFOpenFunc:
 * @filename: The filename to open
 * @mode: fopen() modes for opening @filename
 * 
 * A function following the fclose() semantic, used to close a #FILE
 * object.
 * 
 * Returns: A new #FILE object, or %NULL on failure
 */
typedef FILE    *(* MIOFOpenFunc)   (const char *filename,
                                     const char *mode);

/**
 * MIOFCloseFunc:
 * @fp: An opened #FILE object
 * 
 * A function following the fclose() semantic, used to close a #FILE
 * object.
 * 
 * Returns: 0 on success, EOF otherwise.
 */
typedef int      (* MIOFCloseFunc)  (FILE *fp);

/**
 * MIOPos:
 * 
 * An object representing the state of a #MIO stream. This object can be
 * statically allocated but all its fields are private and should not be
 * accessed directly.
 */
struct _MIOPos {
  /*< private >*/
  unsigned int type;
#ifdef MIO_DEBUG
  void *tag;
#endif
  union {
    fpos_t file;
    size_t mem;
  } impl;
};

/**
 * MIO:
 * 
 * An object representing a #MIO stream. No assumptions should be made about
 * what compose this object, and none of its fields should be accessed directly.
 */
struct _MIO {
  /*< private >*/
  unsigned int type;
  union {
    struct {
      FILE           *fp;
      MIOFCloseFunc   close_func;
    } file;
    struct {
      unsigned char  *buf;
      int             ungetch;
      size_t          pos;
      size_t          size;
      size_t          allocated_size;
      MIOReallocFunc  realloc_func;
      MIOFreeFunc     free_func;
      /* flags */
      /* FIXME: these could be 1-bit bitfields, but it would break the ABI
       * since it would change the size of the structure */
      unsigned int    error;
      unsigned int    eof;
    } mem;
  } impl;
  /* virtual function table */
  void    (*v_free)     (MIO *mio);
  size_t  (*v_read)     (MIO     *mio,
                         void    *ptr,
                         size_t   size,
                         size_t   nmemb);
  size_t  (*v_write)    (MIO         *mio,
                         const void  *ptr,
                         size_t       size,
                         size_t       nmemb);
  int     (*v_getc)     (MIO *mio);
  char   *(*v_gets)     (MIO   *mio,
                         char  *s,
                         size_t size);
  int     (*v_ungetc)   (MIO *mio,
                         int  ch);
  int     (*v_putc)     (MIO *mio,
                         int  c);
  int     (*v_puts)     (MIO         *mio,
                         const char  *s);
  int     (*v_vprintf)  (MIO         *mio,
                         const char  *format,
                         va_list      ap) __attribute__((__format__ (__printf__, 2, 0)));
  void    (*v_clearerr) (MIO *mio);
  int     (*v_eof)      (MIO *mio);
  int     (*v_error)    (MIO *mio);
  int     (*v_seek)     (MIO   *mio,
                         long   offset,
                         int    whence);
  long    (*v_tell)     (MIO *mio);
  void    (*v_rewind)   (MIO *mio);
  int     (*v_getpos)   (MIO     *mio,
                         MIOPos  *pos);
  int     (*v_setpos)   (MIO     *mio,
                         MIOPos  *pos);
};


MIO            *mio_new_file            (const char  *filename,
                                         const char  *mode);
MIO            *mio_new_file_full       (const char    *filename,
                                         const char    *mode,
                                         MIOFOpenFunc   open_func,
                                         MIOFCloseFunc  close_func);
MIO            *mio_new_fp              (FILE          *fp,
                                         MIOFCloseFunc  close_func);
MIO            *mio_new_memory          (unsigned char *data,
                                         size_t         size,
                                         MIOReallocFunc realloc_func,
                                         MIOFreeFunc    free_func);
void            mio_free                (MIO *mio);
FILE           *mio_file_get_fp         (MIO *mio);
unsigned char  *mio_memory_get_data     (MIO     *mio,
                                         size_t  *size);
size_t          mio_read                (MIO     *mio,
                                         void    *ptr,
                                         size_t   size,
                                         size_t   nmemb);
size_t          mio_write               (MIO         *mio,
                                         const void  *ptr,
                                         size_t       size,
                                         size_t       nmemb);
int             mio_getc                (MIO *mio);
char           *mio_gets                (MIO   *mio,
                                         char  *s,
                                         size_t size);
int             mio_ungetc              (MIO *mio,
                                         int  ch);
int             mio_putc                (MIO *mio,
                                         int  c);
int             mio_puts                (MIO         *mio,
                                         const char  *s);

int             mio_vprintf             (MIO         *mio,
                                         const char  *format,
                                         va_list      ap) __attribute__((__format__ (__printf__, 2, 0)));
int             mio_printf              (MIO         *mio,
                                         const char  *format,
                                         ...) __attribute__((__format__ (__printf__, 2, 3)));

void            mio_clearerr            (MIO *mio);
int             mio_eof                 (MIO *mio);
int             mio_error               (MIO *mio);
int             mio_seek                (MIO   *mio,
                                         long   offset,
                                         int    whence);
long            mio_tell                (MIO *mio);
void            mio_rewind              (MIO *mio);
int             mio_getpos              (MIO     *mio,
                                         MIOPos  *pos);
int             mio_setpos              (MIO     *mio,
                                         MIOPos  *pos);



#endif /* guard */
