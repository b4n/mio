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

#include "mio.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>



MIO *
mio_new_file (const gchar *path,
              const gchar *mode)
{
  FILE *fp;
  MIO  *mio = NULL;
  
  fp = fopen (path, mode);
  if (fp) {
    mio = g_slice_alloc (sizeof *mio);
    if (! mio) {
      fclose (fp);
    } else {
      mio->type = MIO_TYPE_FILE;
      mio->impl.file.fp = fp;
      mio->impl.file.close = TRUE;
    }
  }
  
  return mio;
}

MIO *
mio_new_fp (FILE     *fp,
            gboolean  do_close)
{
  MIO *mio;
  
  mio = g_slice_alloc (sizeof *mio);
  if (mio) {
    mio->type = MIO_TYPE_FILE;
    mio->impl.file.fp = fp;
    mio->impl.file.close = do_close;
  }
  
  return mio;
}

MIO *
mio_new_memory (guchar         *data,
                gsize           size,
                MIOReallocFunc  realloc_func,
                GDestroyNotify  free_func)
{
  MIO  *mio;
  
  mio = g_slice_alloc (sizeof *mio);
  if (mio) {
    mio->type = MIO_TYPE_MEMORY;
    mio->impl.mem.buf = data;
    mio->impl.mem.ungetch = EOF;
    mio->impl.mem.pos = 0;
    mio->impl.mem.size = size;
    mio->impl.mem.allocated_size = size;
    mio->impl.mem.realloc_func = realloc_func;
    mio->impl.mem.free_func = free_func;
  }
  
  return mio;
}

void
mio_free (MIO *mio)
{
  if (mio) {
    switch (mio->type) {
      case MIO_TYPE_MEMORY:
        if (mio->impl.mem.free_func) {
          mio->impl.mem.free_func (mio->impl.mem.buf);
        }
        mio->impl.mem.buf = NULL;
        mio->impl.mem.pos = 0;
        mio->impl.mem.size = 0;
        mio->impl.mem.allocated_size = 0;
        mio->impl.mem.realloc_func = NULL;
        mio->impl.mem.free_func = NULL;
        break;
      
      case MIO_TYPE_FILE:
        if (mio->impl.file.close) {
          fclose (mio->impl.file.fp);
        }
        mio->impl.file.close = FALSE;
        mio->impl.file.fp = NULL;
        break;
    }
    g_slice_free1 (sizeof *mio, mio);
  }
}

gsize
mio_read (MIO    *mio,
          void   *ptr,
          gsize   size,
          gsize   nmemb)
{
  gsize n_read = 0;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (mio->impl.mem.ungetch != EOF && nmemb > 0) {
        *((guchar *)ptr) = (guchar)mio->impl.mem.ungetch;
        mio->impl.mem.ungetch = EOF;
        if (size == 1) {
          n_read++;
        } else if (mio->impl.mem.pos + (size - 1) <= mio->impl.mem.size) {
          memcpy (&(((guchar *)ptr)[1]),
                  &mio->impl.mem.buf[mio->impl.mem.pos], size - 1);
          mio->impl.mem.pos += size - 1;
          n_read++;
        }
      }
      for (; n_read < nmemb; n_read++) {
        if (mio->impl.mem.pos + size > mio->impl.mem.size) {
          break;
        } else {
          memcpy (&(((guchar *)ptr)[n_read * size]),
                  &mio->impl.mem.buf[mio->impl.mem.pos], size);
          mio->impl.mem.pos += size;
        }
      }
      break;
    
    case MIO_TYPE_FILE:
      n_read = fread (ptr, size, nmemb, mio->impl.file.fp);
      break;
  }
  
  return n_read;
}

gint
mio_getc (MIO *mio)
{
  gint rv = EOF;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (mio->impl.mem.ungetch != EOF) {
        rv = mio->impl.mem.ungetch;
        mio->impl.mem.ungetch = EOF;
      } else if (mio->impl.mem.pos < mio->impl.mem.size) {
        rv = mio->impl.mem.buf[mio->impl.mem.pos];
        mio->impl.mem.pos++;
      }
      break;
    
    case MIO_TYPE_FILE:
      rv = fgetc (mio->impl.file.fp);
      break;
  }
  
  return rv;
}

gint
mio_ungetc (MIO  *mio,
            gint  ch)
{
  gint rv = EOF;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (mio->impl.mem.ungetch == EOF) {
        rv = mio->impl.mem.ungetch = ch;
      }
      break;
    
    case MIO_TYPE_FILE:
      rv = ungetc (ch, mio->impl.file.fp);
      break;
  }
  
  return rv;
}

gchar *
mio_gets (MIO    *mio,
          gchar  *s,
          gsize   size)
{
  gchar *rv = NULL;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (size > 0) {
        gsize i = 0;
        
        if (mio->impl.mem.ungetch != EOF) {
          s[i] = (gchar)mio->impl.mem.ungetch;
          mio->impl.mem.ungetch = EOF;
          i++;
        }
        for (; mio->impl.mem.pos < mio->impl.mem.size && i < (size - 1); i++) {
          s[i] = (gchar)mio->impl.mem.buf[mio->impl.mem.pos];
          mio->impl.mem.pos++;
          if (s[i] == '\n') {
            i++;
            break;
          }
        }
        if (i > 0) {
          s[i] = 0;
          rv = s;
        }
      }
      break;
    
    case MIO_TYPE_FILE:
      if (size > G_MAXINT) {
        /* FIXME: report the error */
      } else {
        rv = fgets (s, (int)size, mio->impl.file.fp);
      }
      break;
  }
  
  return rv;
}
