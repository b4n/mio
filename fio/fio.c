/*
 *  FIO, an I/O abstraction layer replicating C file I/O API.
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

#include "fio.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>



FIO *
fio_new_file (const gchar *path,
              const gchar *mode)
{
  FILE *fp;
  FIO  *fio = NULL;
  
  fp = fopen (path, mode);
  if (fp) {
    fio = g_slice_alloc (sizeof *fio);
    if (! fio) {
      fclose (fp);
    } else {
      fio->type = FIO_TYPE_FILE;
      fio->impl.file.fp = fp;
      fio->impl.file.close = TRUE;
    }
  }
  
  return fio;
}

FIO *
fio_new_fp (FILE     *fp,
            gboolean  do_close)
{
  FIO *fio;
  
  fio = g_slice_alloc (sizeof *fio);
  if (fio) {
    fio->type = FIO_TYPE_FILE;
    fio->impl.file.fp = fp;
    fio->impl.file.close = do_close;
  }
  
  return fio;
}

FIO *
fio_new_memory (guchar         *data,
                gsize           size,
                FIOReallocFunc  realloc_func)
{
  FIO  *fio;
  
  fio = g_slice_alloc (sizeof *fio);
  if (fio) {
    fio->type = FIO_TYPE_MEMORY;
    fio->impl.mem.buf = data;
    fio->impl.mem.pos = 0;
    fio->impl.mem.size = size;
    fio->impl.mem.allocated_size = size;
    fio->impl.mem.realloc_func = realloc_func;
  }
  
  return fio;
}

void
fio_free (FIO *fio)
{
  if (fio) {
    switch (fio->type) {
      case FIO_TYPE_MEMORY:
        if (fio->impl.mem.realloc_func) {
          fio->impl.mem.realloc_func (fio->impl.mem.buf, 0);
        }
        fio->impl.mem.buf = NULL;
        fio->impl.mem.pos = 0;
        fio->impl.mem.size = 0;
        fio->impl.mem.allocated_size = 0;
        fio->impl.mem.realloc_func = NULL;
        break;
      
      case FIO_TYPE_FILE:
        if (fio->impl.file.close) {
          fclose (fio->impl.file.fp);
        }
        fio->impl.file.close = FALSE;
        fio->impl.file.fp = NULL;
        break;
    }
    g_slice_free1 (sizeof *fio, fio);
  }
}

gsize
fio_read (FIO    *fio,
          void   *ptr,
          gsize   size,
          gsize   nmemb)
{
  gsize n_read = 0;
  
  switch (fio->type) {
    case FIO_TYPE_MEMORY:
      for (n_read = 0; n_read < nmemb; n_read++) {
        if (fio->impl.mem.pos + size >= fio->impl.mem.size) {
          /* FIXME: set the error */
          break;
        } else {
          memcpy (&(((guchar *)ptr)[n_read * size]),
                  &fio->impl.mem.buf[fio->impl.mem.pos], size);
          fio->impl.mem.pos += size;
        }
      }
      break;
    
    case FIO_TYPE_FILE:
      n_read = fread (ptr, size, nmemb, fio->impl.file.fp);
      break;
  }
  
  return n_read;
}

gint
fio_getc (FIO *fio)
{
  gint rv = EOF;
  
  switch (fio->type) {
    case FIO_TYPE_MEMORY:
      if (fio->impl.mem.pos < fio->impl.mem.size) {
        rv = fio->impl.mem.buf[fio->impl.mem.pos];
        fio->impl.mem.pos++;
      }
      break;
    
    case FIO_TYPE_FILE:
      rv = fgetc (fio->impl.file.fp);
      break;
  }
  
  return rv;
}

gchar *
fio_gets (FIO    *fio,
          gchar  *s,
          gsize   size)
{
  gchar *rv = NULL;
  
  switch (fio->type) {
    case FIO_TYPE_MEMORY:
      if (fio->impl.mem.pos + (size - 1) < fio->impl.mem.size) {
        memcpy (s, &fio->impl.mem.buf[fio->impl.mem.pos], size - 1);
        fio->impl.mem.pos += size - 1;
        s[size - 1] = 0;
        rv = s;
      }
      break;
    
    case FIO_TYPE_FILE:
      if (size > G_MAXINT) {
        /* FIXME: report the error */
      } else {
        rv = fgets (s, (int)size, fio->impl.file.fp);
      }
      break;
  }
  
  return rv;
}
