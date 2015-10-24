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

/* file IO implementation */

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#include "mio.h"


#define FILE_SET_VTABLE(mio)          \
  do {                                \
    mio->v_free     = file_free;      \
    mio->v_read     = file_read;      \
    mio->v_write    = file_write;     \
    mio->v_getc     = file_getc;      \
    mio->v_gets     = file_gets;      \
    mio->v_ungetc   = file_ungetc;    \
    mio->v_putc     = file_putc;      \
    mio->v_puts     = file_puts;      \
    mio->v_vprintf  = file_vprintf;   \
    mio->v_clearerr = file_clearerr;  \
    mio->v_eof      = file_eof;       \
    mio->v_error    = file_error;     \
    mio->v_seek     = file_seek;      \
    mio->v_tell     = file_tell;      \
    mio->v_rewind   = file_rewind;    \
    mio->v_getpos   = file_getpos;    \
    mio->v_setpos   = file_setpos;    \
  } while (0)


static void
file_free (MIO *mio)
{
  if (mio->impl.file.close_func) {
    mio->impl.file.close_func (mio->impl.file.fp);
  }
  mio->impl.file.close_func = NULL;
  mio->impl.file.fp = NULL;
}

static size_t
file_read (MIO    *mio,
           void   *ptr,
           size_t  size,
           size_t  nmemb)
{
  return fread (ptr, size, nmemb, mio->impl.file.fp);
}

static size_t
file_write (MIO         *mio,
            const void  *ptr,
            size_t       size,
            size_t       nmemb)
{
  return fwrite (ptr, size, nmemb, mio->impl.file.fp);
}

static int
file_putc (MIO  *mio,
           int   c)
{
  return fputc (c, mio->impl.file.fp);
}

static int
file_puts (MIO        *mio,
           const char *s)
{
  return fputs (s, mio->impl.file.fp);
}

__attribute__((__format__ (__printf__, 2, 0)))
static int
file_vprintf (MIO         *mio,
              const char  *format,
              va_list      ap)
{
  return vfprintf (mio->impl.file.fp, format, ap);
}

static int
file_getc (MIO *mio)
{
  return fgetc (mio->impl.file.fp);
}

static int
file_ungetc (MIO  *mio,
             int   ch)
{
  return ungetc (ch, mio->impl.file.fp);
}

static char *
file_gets (MIO    *mio,
           char   *s,
           size_t  size)
{
  return fgets (s, (int)size, mio->impl.file.fp);
}

static void
file_clearerr (MIO *mio)
{
  clearerr (mio->impl.file.fp);
}

static int
file_eof (MIO *mio)
{
  return feof (mio->impl.file.fp);
}

static int
file_error (MIO *mio)
{
  return ferror (mio->impl.file.fp);
}

static int
file_seek (MIO  *mio,
           long  offset,
           int   whence)
{
  return fseek (mio->impl.file.fp, offset, whence);
}

static long
file_tell (MIO *mio)
{
  return ftell (mio->impl.file.fp);
}

static void
file_rewind (MIO *mio)
{
  rewind (mio->impl.file.fp);
}

static int
file_getpos (MIO    *mio,
             MIOPos *pos)
{
  return fgetpos (mio->impl.file.fp, &pos->impl.file);
}

static int
file_setpos (MIO    *mio,
             MIOPos *pos)
{
  return fsetpos (mio->impl.file.fp, &pos->impl.file);
}
