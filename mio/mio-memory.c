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

/* memory IO implementation */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_GLIB
# include <glib.h>
# define LIKELY(expr)   G_LIKELY(expr)
# define UNLIKELY(expr) G_UNLIKELY(expr)
#elif defined (__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
# define LIKELY(expr)   (__builtin_expect ((expr), 1))
# define UNLIKELY(expr) (__builtin_expect ((expr), 0))
#else
# define LIKELY(expr)   (expr)
# define UNLIKELY(expr) (expr)
#endif
#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mio.h"

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

#ifndef MAX
# define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif


#define MEM_SET_VTABLE(mio)           \
  do {                                \
    mio->v_free     = mem_free;       \
    mio->v_read     = mem_read;       \
    mio->v_write    = mem_write;      \
    mio->v_getc     = mem_getc;       \
    mio->v_gets     = mem_gets;       \
    mio->v_ungetc   = mem_ungetc;     \
    mio->v_putc     = mem_putc;       \
    mio->v_puts     = mem_puts;       \
    mio->v_vprintf  = mem_vprintf;    \
    mio->v_clearerr = mem_clearerr;   \
    mio->v_eof      = mem_eof;        \
    mio->v_error    = mem_error;      \
    mio->v_seek     = mem_seek;       \
    mio->v_tell     = mem_tell;       \
    mio->v_rewind   = mem_rewind;     \
    mio->v_getpos   = mem_getpos;     \
    mio->v_setpos   = mem_setpos;     \
  } while (0)


/* minimal reallocation chunk size */
#define MIO_CHUNK_SIZE 4096


static void
mem_free (MIO *mio)
{
  if (mio->impl.mem.free_func) {
    mio->impl.mem.free_func (mio->impl.mem.buf);
  }
  mio->impl.mem.buf = NULL;
  mio->impl.mem.pos = 0;
  mio->impl.mem.size = 0;
  mio->impl.mem.allocated_size = 0;
  mio->impl.mem.realloc_func = NULL;
  mio->impl.mem.free_func = NULL;
  mio->impl.mem.eof = FALSE;
  mio->impl.mem.error = FALSE;
}

static size_t
mem_read (MIO    *mio,
          void   *ptr_,
          size_t  size,
          size_t  nmemb)
{
  size_t n_read = 0;
  
  if (size != 0 && nmemb != 0) {
    size_t          size_avail  = mio->impl.mem.size - mio->impl.mem.pos;
    size_t          copy_bytes  = size * nmemb;
    unsigned char  *ptr         = ptr_;
    
    if (size_avail < copy_bytes) {
      copy_bytes = size_avail;
    }
    
    if (copy_bytes > 0) {
      n_read = copy_bytes / size;
      
      if (mio->impl.mem.ungetch != EOF) {
        *ptr = (unsigned char) mio->impl.mem.ungetch;
        mio->impl.mem.ungetch = EOF;
        copy_bytes--;
        mio->impl.mem.pos++;
        ptr++;
      }
      
      memcpy (ptr, &mio->impl.mem.buf[mio->impl.mem.pos], copy_bytes);
      mio->impl.mem.pos += copy_bytes;
    }
    if (mio->impl.mem.pos >= mio->impl.mem.size) {
      mio->impl.mem.eof = TRUE;
    }
  }
  
  return n_read;
}

/*
 * mem_try_resize:
 * @mio: A #MIO object of the type %MIO_TYPE_MEMORY
 * @new_size: Requested new size
 * 
 * Tries to resize the underlying buffer of an in-memory #MIO object.
 * This supports both growing and shrinking.
 * 
 * Returns: %TRUE on success, %FALSE otherwise.
 */
static int
mem_try_resize (MIO    *mio,
                size_t  new_size)
{
  int success = FALSE;
  
  if (mio->impl.mem.realloc_func) {
    if (UNLIKELY (new_size == ((size_t) -1))) {
      #ifdef EOVERFLOW
      errno = EOVERFLOW;
      #endif
    } else {
      if (new_size > mio->impl.mem.size) {
        if (new_size <= mio->impl.mem.allocated_size) {
          mio->impl.mem.size = new_size;
          success = TRUE;
        } else {
          size_t          newsize;
          unsigned char  *newbuf;
          
          newsize = MAX (mio->impl.mem.allocated_size + MIO_CHUNK_SIZE,
                         new_size);
          newbuf = mio->impl.mem.realloc_func (mio->impl.mem.buf, newsize);
          if (newbuf) {
            mio->impl.mem.buf = newbuf;
            mio->impl.mem.allocated_size = newsize;
            mio->impl.mem.size = new_size;
            success = TRUE;
          }
        }
      } else {
        unsigned char *newbuf;
        
        newbuf = mio->impl.mem.realloc_func (mio->impl.mem.buf, new_size);
        if (LIKELY (newbuf || new_size == 0)) {
          mio->impl.mem.buf = newbuf;
          mio->impl.mem.allocated_size = new_size;
          mio->impl.mem.size = new_size;
          success = TRUE;
        }
      }
    }
  }
  
  return success;
}

/*
 * mem_try_ensure_space:
 * @mio: A #MIO object
 * @n: Requested size from the current (cursor) position
 * 
 * Tries to ensure there is enough space for @n bytes to be written from the
 * current cursor position.
 * 
 * Returns: %TRUE if there is enough space, %FALSE otherwise.
 */
static int
mem_try_ensure_space (MIO    *mio,
                      size_t  n)
{
  int success = TRUE;
  
  if (mio->impl.mem.pos + n > mio->impl.mem.size) {
    success = mem_try_resize (mio, mio->impl.mem.pos + n);
  }
  
  return success;
}

static size_t
mem_write (MIO         *mio,
           const void  *ptr,
           size_t       size,
           size_t       nmemb)
{
  size_t n_written = 0;
  
  if (size != 0 && nmemb != 0) {
    if (mem_try_ensure_space (mio, size * nmemb)) {
      memcpy (&mio->impl.mem.buf[mio->impl.mem.pos], ptr, size * nmemb);
      mio->impl.mem.pos += size * nmemb;
      n_written = nmemb;
    }
  }
  
  return n_written;
}

static int
mem_putc (MIO  *mio,
          int   c)
{
  int rv = EOF;
  
  if (mem_try_ensure_space (mio, 1)) {
    mio->impl.mem.buf[mio->impl.mem.pos] = (unsigned char) c;
    mio->impl.mem.pos++;
    rv = (int) ((unsigned char) c);
  }
  
  return rv;
}

static int
mem_puts (MIO        *mio,
          const char *s)
{
  int     rv = EOF;
  size_t  len;
  
  len = strlen (s);
  if (mem_try_ensure_space (mio, len)) {
    memcpy (&mio->impl.mem.buf[mio->impl.mem.pos], s, len);
    mio->impl.mem.pos += len;
    rv = 1;
  }
  
  return rv;
}

static int
mem_vprintf (MIO         *mio,
             const char  *format,
             va_list      ap)
{
  int     rv = -1;
  size_t  n;
  size_t  old_pos;
  size_t  old_size;
  va_list ap_copy;
#ifndef HAVE_GLIB
  char    dummy;
#endif
  
  old_pos = mio->impl.mem.pos;
  old_size = mio->impl.mem.size;
  /* compute the size we will need into the buffer */
#ifndef HAVE_GLIB
  va_copy (ap_copy, ap);
  n = (size_t) vsnprintf (&dummy, 1, format, ap_copy) + 1;
#else
  G_VA_COPY (ap_copy, ap);
  n = g_printf_string_upper_bound (format, ap_copy);
#endif
  va_end (ap_copy);
  if (mem_try_ensure_space (mio, n)) {
    unsigned char c;
    
    /* backup character at n+1 that will be overwritten by a \0 ... */
    c = mio->impl.mem.buf[mio->impl.mem.pos + (n - 1)];
    rv = vsprintf ((char *) &mio->impl.mem.buf[mio->impl.mem.pos], format, ap);
    /* ...and restore it */
    mio->impl.mem.buf[mio->impl.mem.pos + (n - 1)] = c;
    if (LIKELY (rv >= 0 && (size_t) rv == (n - 1))) {
      /* re-compute the actual size since we might have allocated one byte
       * more than needed */
      mio->impl.mem.size = MAX (old_size, old_pos + (unsigned int) rv);
      mio->impl.mem.pos += (unsigned int) rv;
    } else {
      mio->impl.mem.size = old_size;
      rv = -1;
    }
  }
  
  return rv;
}

static int
mem_getc (MIO *mio)
{
  int rv = EOF;
  
  if (mio->impl.mem.ungetch != EOF) {
    rv = mio->impl.mem.ungetch;
    mio->impl.mem.ungetch = EOF;
    mio->impl.mem.pos++;
  } else if (mio->impl.mem.pos < mio->impl.mem.size) {
    rv = mio->impl.mem.buf[mio->impl.mem.pos];
    mio->impl.mem.pos++;
  } else {
    mio->impl.mem.eof = TRUE;
  }
  
  return rv;
}

static int
mem_ungetc (MIO  *mio,
            int   ch)
{
  int rv = EOF;
  
  if (ch != EOF && mio->impl.mem.ungetch == EOF) {
    rv = mio->impl.mem.ungetch = ch;
    mio->impl.mem.pos--;
    mio->impl.mem.eof = FALSE;
  }
  
  return rv;
}

static char *
mem_gets (MIO    *mio,
          char   *s,
          size_t  size)
{
  char *rv = NULL;
  
  if (size > 0) {
    size_t i = 0;
    
    if (mio->impl.mem.ungetch != EOF) {
      s[i] = (char) mio->impl.mem.ungetch;
      mio->impl.mem.ungetch = EOF;
      mio->impl.mem.pos++;
      i++;
    }
    for (; mio->impl.mem.pos < mio->impl.mem.size && i < (size - 1); i++) {
      s[i] = (char) mio->impl.mem.buf[mio->impl.mem.pos];
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
    if (mio->impl.mem.pos >= mio->impl.mem.size) {
      mio->impl.mem.eof = TRUE;
    }
  }
  
  return rv;
}

static void
mem_clearerr (MIO *mio)
{
  mio->impl.mem.error = FALSE;
  mio->impl.mem.eof = FALSE;
}

static int
mem_eof (MIO *mio)
{
  return mio->impl.mem.eof != FALSE;
}

static int
mem_error (MIO *mio)
{
  return mio->impl.mem.error != FALSE;
}

/* FIXME: should we support seeking out of bounds like lseek() seems to do? */
static int
mem_seek (MIO  *mio,
          long  offset,
          int   whence)
{
  int rv = -1;
  
  switch (whence) {
    case SEEK_SET:
      if (offset < 0 || (size_t) offset > mio->impl.mem.size) {
        errno = EINVAL;
      } else {
        mio->impl.mem.pos = (size_t) offset;
        rv = 0;
      }
      break;
    
    case SEEK_CUR:
      if ((offset < 0 && (size_t) -offset > mio->impl.mem.pos) ||
          mio->impl.mem.pos + (size_t) offset > mio->impl.mem.size) {
        errno = EINVAL;
      } else {
        mio->impl.mem.pos = (size_t) ((ssize_t) mio->impl.mem.pos + offset);
        rv = 0;
      }
      break;
    
    case SEEK_END:
      if (offset > 0 || (size_t) -offset > mio->impl.mem.size) {
        errno = EINVAL;
      } else {
        mio->impl.mem.pos = mio->impl.mem.size - (size_t) -offset;
        rv = 0;
      }
      break;
    
    default:
      errno = EINVAL;
  }
  if (rv == 0) {
    mio->impl.mem.eof = FALSE;
    mio->impl.mem.ungetch = EOF;
  }
  
  return rv;
}

static long
mem_tell (MIO *mio)
{
  long rv = -1;
  
  if (mio->impl.mem.pos > LONG_MAX) {
    #ifdef EOVERFLOW
    errno = EOVERFLOW;
    #endif
  } else {
    rv = (long) mio->impl.mem.pos;
  }
  
  return rv;
}

static void
mem_rewind (MIO *mio)
{
  mio->impl.mem.pos = 0;
  mio->impl.mem.ungetch = EOF;
  mio->impl.mem.eof = FALSE;
  mio->impl.mem.error = FALSE;
}

static int
mem_getpos (MIO    *mio,
            MIOPos *pos)
{
  int rv = -1;
  
  if (mio->impl.mem.pos == (size_t) -1) {
    /* this happens if ungetc() was called at the start of the stream */
    #ifdef EIO
    errno = EIO;
    #endif
  } else {
    pos->impl.mem = mio->impl.mem.pos;
    rv = 0;
  }
  
  return rv;
}

static int
mem_setpos (MIO    *mio,
            MIOPos *pos)
{
  int rv = -1;
  
  if (pos->impl.mem > mio->impl.mem.size) {
    errno = EINVAL;
  } else {
    mio->impl.mem.ungetch = EOF;
    mio->impl.mem.pos = pos->impl.mem;
    rv = 0;
  }
  
  return rv;
}
