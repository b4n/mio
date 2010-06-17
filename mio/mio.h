/*
 *  MIO, an I/O abstraction layer replicating C file I/O API.
 *  Copyright (C) 2010  Colomban Wendling <ban@herbesfolles.org>
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef H_MIO_H
#define H_MIO_H

#include <glib.h>
#include <stdio.h>


enum MIOType {
  MIO_TYPE_FILE,
  MIO_TYPE_MEMORY
};

typedef enum MIOType    MIOType;
typedef struct MIO      MIO;
typedef struct MIOPos   MIOPos;
/* should be GReallocFunc but it's only defined by GIO */
typedef gpointer (* MIOReallocFunc) (gpointer data,
                                     gsize    size);

struct MIOPos {
  guint type;
#ifdef MIO_DEBUG
  void *tag;
#endif
  union {
    fpos_t file;
    size_t mem;
  } impl;
};

struct MIO {
  guint type;
  union {
    struct {
      FILE       *fp;
      gboolean    close;
    } file;
    struct {
      guchar         *buf;
      gint            ungetch;
      gsize           pos;
      gsize           size;
      gsize           allocated_size;
      MIOReallocFunc  realloc_func;
      GDestroyNotify  free_func;
    } mem;
  } impl;
};


MIO        *mio_new_file    (const gchar *path,
                             const gchar *mode);
MIO        *mio_new_fp      (FILE        *fp,
                             gboolean     do_close);
MIO        *mio_new_memory  (guchar        *data,
                             gsize          size,
                             MIOReallocFunc realloc_func,
                             GDestroyNotify free_func);
void        mio_free        (MIO *mio);
gsize       mio_read        (MIO     *mio,
                             void    *ptr,
                             gsize    size,
                             gsize    nmemb);
gsize       mio_write       (MIO         *mio,
                             const void  *ptr,
                             gsize        size,
                             gsize        nmemb);
gint        mio_getc        (MIO *mio);
gchar      *mio_gets        (MIO   *mio,
                             gchar *s,
                             gsize  size);
gint        mio_ungetc      (MIO *mio,
                             gint c);
gint        mio_putc        (MIO *mio,
                             gint c);
gint        mio_puts        (MIO         *mio,
                             const gchar *s);

gint        mio_clearerr    (MIO *mio);
gint        mio_eof         (MIO *mio);
gint        mio_error       (MIO *mio);
gint        mio_seek        (MIO   *mio,
                             glong  offset,
                             gint   whence);
glong       mio_tell        (MIO *mio);
void        mio_rewind      (MIO *mio);
gint        mio_getpos      (MIO     *mio,
                             MIOPos  *pos);
gint        mio_setpos      (MIO     *mio,
                             MIOPos  *pos);



#endif /* guard */
