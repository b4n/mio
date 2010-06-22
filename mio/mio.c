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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


/* minimal reallocation chunk size */
#define MIO_CHUNK_SIZE 4096



/**
 * mio_new_file:
 * @path: Filename to open, same as the fopen()'s first argument
 * @mode: Mode in which open the file, fopen()'s second argument
 * 
 * Creates a new #MIO object working on a file from a path; wrapping fopen().
 * 
 * Returns: A new #MIO on success or %NULL on failure.
 */
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

/**
 * mio_new_fp:
 * @fp: A libc FILE object
 * @do_close: Whether the file object @fp should be closed with fclose() when
 *            the created #MIO is destroyed
 * 
 * Creates a new #MIO object working on a file, from an already opened FILE
 * object.
 * 
 * Returns: A new #MIO on success or %NULL on failure.
 */
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

/**
 * mio_new_memory:
 * @data: Initial data (may be %NULL)
 * @size: Length of @data in bytes
 * @realloc_func: A function with the realloc() semantic used to grow the
 *                buffer, or %NULL to disable buffer growing
 * @free_func: A function with the free() semantic to destroy the data together
 *             with the object, or %NULL not to destroy the data
 * 
 * Creates a new #MIO object working on memory.
 * 
 * To allow the buffer to grow, you must provide a @realloc_func, otherwise
 * trying to write after the end of the current data will fail.
 * 
 * If you want the buffer to be freed together with the #MIO object, you must
 * give a @free_func; otherwise the data will still live after #MIO object
 * termination.
 * 
 * <example>
 * <title>Basic creation of a non-growable, freeable #MIO object</title>
 * <programlisting>
 * MIO *mio = mio_new_memory (data, size, NULL, g_free);
 * </programlisting>
 * </example>
 * 
 * <example>
 * <title>Basic creation of an empty growable and freeable #MIO object</title>
 * <programlisting>
 * MIO *mio = mio_new_memory (NULL, 0, g_try_realloc, g_free);
 * </programlisting>
 * </example>
 * 
 * Returns: A new #MIO on success, or %NULL on failure.
 */
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
    mio->impl.mem.eof = FALSE;
    mio->impl.mem.error = FALSE;
  }
  
  return mio;
}

/**
 * mio_free:
 * @mio: A #MIO object
 * 
 * Destroys a #MIO object.
 */
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
        mio->impl.mem.eof = FALSE;
        mio->impl.mem.error = FALSE;
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

/**
 * mio_read:
 * @mio: A #MIO object
 * @ptr: Pointer to the memory to fill with the read data
 * @size: Size of each block to read
 * @nmemb: Number o blocks to read
 * 
 * Reads raw data from a #MIO stream. This function behave the same as fread().
 * 
 * Returns: The number of actually read blocks. If an error occurs or if the
 *          end of the stream is reached, the return value may be smaller than
 *          the requested block count, or even 0. This function doesn't
 *          distinguish between end-of-stream and an error, you should then use
 *          mio_eof() and mio_error() to determine which occurred.
 */
gsize
mio_read (MIO    *mio,
          void   *ptr,
          gsize   size,
          gsize   nmemb)
{
  gsize n_read = 0;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (size != 0 && nmemb != 0) {
        if (mio->impl.mem.ungetch != EOF) {
          *((guchar *)ptr) = (guchar)mio->impl.mem.ungetch;
          mio->impl.mem.ungetch = EOF;
          mio->impl.mem.pos++;
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
        if (mio->impl.mem.pos >= mio->impl.mem.size) {
          mio->impl.mem.eof = TRUE;
        }
      }
      break;
    
    case MIO_TYPE_FILE:
      n_read = fread (ptr, size, nmemb, mio->impl.file.fp);
      break;
  }
  
  return n_read;
}

/*
 * try_resize:
 * @mio: A #MIO object of the type %MIO_TYPE_MEMORY
 * @new_size: Requested new size
 * 
 * Tries to resize the underlying buffer of an in-memory #MIO object.
 * This supports both growing and shrinking.
 * 
 * Returns: %TRUE on success, %FALSE otherwise.
 */
static gboolean
try_resize (MIO  *mio,
            gsize new_size)
{
  gboolean success = FALSE;
  
  if (mio->impl.mem.realloc_func) {
    if (G_UNLIKELY (new_size == G_MAXSIZE)) {
      #ifdef EOVERFLOW
      errno = EOVERFLOW;
      #endif
    } else {
      if (new_size > mio->impl.mem.size) {
        if (new_size <= mio->impl.mem.allocated_size) {
          mio->impl.mem.size = new_size;
          success = TRUE;
        } else {
          gsize   newsize;
          guchar *newbuf;
          
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
        guchar *newbuf;
        
        newbuf = mio->impl.mem.realloc_func (mio->impl.mem.buf, new_size);
        if (G_LIKELY (newbuf || new_size == 0)) {
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
 * try_ensure_space:
 * @mio: A #MIO object
 * @n: Requested size from the current (cursor) position
 * 
 * Tries to ensure there is enough space for @n bytes to be written from the
 * current cursor position.
 * 
 * Returns: %TRUE if there is enough space, %FALSE otherwise.
 */
static gboolean
try_ensure_space (MIO  *mio,
                  gsize n)
{
  gboolean success = TRUE;
  
  if (mio->impl.mem.pos + n > mio->impl.mem.size) {
    success = try_resize (mio, mio->impl.mem.pos + n);
  }
  
  return success;
}

/**
 * mio_write:
 * @mio: A #MIO object
 * @ptr: Pointer to the memory to write on the stream
 * @size: Size of each block to write
 * @nmemb: Number of block to write
 * 
 * Writes raw data to a #MIO stream. This function behaves the same as fwrite().
 * 
 * Returns: The number of blocks actually written to the stream. This might be
 *          smaller than the requested count if a write error occurs.
 */
gsize
mio_write (MIO         *mio,
           const void  *ptr,
           gsize        size,
           gsize        nmemb)
{
  gsize n_written = 0;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (size != 0 && nmemb != 0) {
        if (try_ensure_space (mio, size * nmemb)) {
          memcpy (&mio->impl.mem.buf[mio->impl.mem.pos], ptr, size * nmemb);
          mio->impl.mem.pos += size * nmemb;
          n_written = nmemb;
        }
      }
      break;
    
    case MIO_TYPE_FILE:
      n_written = fwrite (ptr, size, nmemb, mio->impl.file.fp);
      break;
  }
  
  return n_written;
}

/**
 * mio_putc:
 * @mio: A #MIO object
 * @c: The character to write
 * 
 * Writes a character to a #MIO stream. This function behaves the same as
 * fputc().
 * 
 * Returns: The written wharacter, or %EOF on error.
 */
gint
mio_putc (MIO  *mio,
          gint  c)
{
  gint rv = EOF;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (try_ensure_space (mio, 1)) {
        mio->impl.mem.buf[mio->impl.mem.pos] = (guchar)c;
        mio->impl.mem.pos++;
        rv = (gint)((guchar)c);
      }
      break;
    
    case MIO_TYPE_FILE:
      rv = fputc (c, mio->impl.file.fp);
      break;
  }
  
  return rv;
}

/**
 * mio_puts:
 * @mio: A #MIO object
 * @s: The string to write
 * 
 * Writes a string to a #MIO object. This function behaves the same as fputs().
 * 
 * Returns: A non-negative integer on success or %EOF on failure.
 */
gint
mio_puts (MIO          *mio,
          const gchar  *s)
{
  gint rv = EOF;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY: {
      gsize len;
      
      len = strlen (s);
      if (try_ensure_space (mio, len)) {
        memcpy (&mio->impl.mem.buf[mio->impl.mem.pos], s, len);
        mio->impl.mem.pos += len;
        rv = 1;
      }
      break;
    }
    
    case MIO_TYPE_FILE:
      rv = fputs (s, mio->impl.file.fp);
      break;
  }
  
  return rv;
}

/**
 * mio_vprintf:
 * @mio: A #MIO object
 * @format: A printf fomrat string
 * @ap: The variadic argument list for the format
 * 
 * Writes a formatted string into a #MIO stream. This function behaves the same
 * as vfprintf().
 * 
 * Returns: The number of bytes written in the stream, or a negative value on
 *          failure.
 */
gint
mio_vprintf (MIO         *mio,
             const gchar *format,
             va_list      ap)
{
  gint    rv = -1;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY: {
      gint    n;
      gchar   tmp;
      gsize   old_pos;
      gsize   old_size;
      va_list ap_copy;
      
      old_pos = mio->impl.mem.pos;
      old_size = mio->impl.mem.size;
      va_copy (ap_copy, ap);
      /* compute the size we will need into the buffer */
      n = vsnprintf (&tmp, 1, format, ap_copy);
      va_end (ap_copy);
      if (n >= 0 && try_ensure_space (mio, ((guint)n) + 1)) {
        guchar c;
        
        /* backup character at n+1 that will be overwritten by a \0 ... */
        c = mio->impl.mem.buf[mio->impl.mem.pos + (guint)n];
        rv = vsnprintf ((gchar *)&mio->impl.mem.buf[mio->impl.mem.pos],
                        (guint)n + 1, format, ap);
        /* ...and restore it */
        mio->impl.mem.buf[mio->impl.mem.pos + (guint)n] = c;
        if (G_LIKELY (rv >= 0 && rv == n)) {
          /* re-compute the actual size since we might have allocated one byte
           * more than needed */
          mio->impl.mem.size = MAX (old_size, old_pos + (guint)rv);
          mio->impl.mem.pos += (guint)rv;
        } else {
          mio->impl.mem.size = old_size;
          rv = -1;
        }
      }
      break;
    }
    
    case MIO_TYPE_FILE:
      rv = vfprintf (mio->impl.file.fp, format, ap);
      break;
  }
  
  return rv;
}

/**
 * mio_printf:
 * @mio: A #MIO object
 * @format: A print format string
 * @...: Arguments of the format
 * 
 * Writes a formatted string to a #MIO stream. This function behaves the same as
 * fprintf().
 * 
 * Returns: The number of bytes written to the stream, or a negative value on
 *          failure.
 */
gint
mio_printf (MIO         *mio,
            const gchar *format,
            ...)
{
  gint    rv;
  va_list ap;
  
  va_start (ap, format);
  rv = mio_vprintf (mio, format, ap);
  va_end (ap);
  
  return rv;
}

/**
 * mio_getc:
 * @mio: A #MIO object
 * 
 * Gets the current character from a #MIO stream. This function behaves the same
 * as fgetc().
 * 
 * Returns: The read character as a #gint, or %EOF on error.
 */
gint
mio_getc (MIO *mio)
{
  gint rv = EOF;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
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
      break;
    
    case MIO_TYPE_FILE:
      rv = fgetc (mio->impl.file.fp);
      break;
  }
  
  return rv;
}

/**
 * mio_ungetc:
 * @mio: A #MIO object
 * @ch: Character to put back in the stream
 * 
 * Puts a character back in a #MIO stream. This function behaves the sames as
 * ungetc().
 * 
 * <warning><para>It is only guaranteed that one character can be but back at a
 * time, even if the implementation may allow more.</para></warning>
 * <warning><para>Using this function while the stream cursor is at offset 0 is
 * not guaranteed to function properly. As the C99 standard says, it is "an
 * obsolescent feature".</para></warning>
 * 
 * Returns: The character put back, or %EOF on error.
 */
gint
mio_ungetc (MIO  *mio,
            gint  ch)
{
  gint rv = EOF;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (ch != EOF && mio->impl.mem.ungetch == EOF) {
        rv = mio->impl.mem.ungetch = ch;
        mio->impl.mem.pos--;
        mio->impl.mem.eof = FALSE;
      }
      break;
    
    case MIO_TYPE_FILE:
      rv = ungetc (ch, mio->impl.file.fp);
      break;
  }
  
  return rv;
}

/**
 * mio_gets:
 * @mio: A #MIO object
 * @s: A string to fill with the read data
 * @size: The maximum number of bytes to read
 * 
 * Reads a string from a #MIO stream, stopping after the first new-line
 * character or at the end of the stream. This function behaves the same as
 * fgets().
 * 
 * Returns: @s on success, %NULL otherwise.
 */
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
          mio->impl.mem.pos++;
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
        if (mio->impl.mem.pos >= mio->impl.mem.size) {
          mio->impl.mem.eof = TRUE;
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

/**
 * mio_clearerr:
 * @mio: A #MIO object
 * 
 * Clears the error and end-of-stream indicators of a #MIO stream. This function
 * behaves the same as clearerr().
 */
void
mio_clearerr (MIO *mio)
{
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      mio->impl.mem.error = FALSE;
      mio->impl.mem.eof = FALSE;
      break;
    
    case MIO_TYPE_FILE:
      clearerr (mio->impl.file.fp);
      break;
  }
}

/**
 * mio_eof:
 * @mio: A #MIO object
 * 
 * Checks whether the end-of-stream indicator of a #MIO stream is set. This
 * function behaves the same as feof().
 * 
 * Returns: A non-null value if the stream reached its end, 0 otherwise.
 */
gint
mio_eof (MIO *mio)
{
  gint rv = 1;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      rv = mio->impl.mem.eof != FALSE;
      break;
    
    case MIO_TYPE_FILE:
      rv = feof (mio->impl.file.fp);
      break;
  }
  
  return rv;
}

/**
 * mio_error:
 * @mio: A #MIO object
 * 
 * Checks whether the error indicator of a #MIO stream is set. This function
 * behaves the same as ferror().
 * 
 * Returns: A non-null value if the stream have an error set, 0 otherwise.
 */
gint
mio_error (MIO *mio)
{
  gint rv = 1;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      rv = mio->impl.mem.error != FALSE;
      break;
    
    case MIO_TYPE_FILE:
      rv = ferror (mio->impl.file.fp);
      break;
  }
  
  return rv;
}

/**
 * mio_seek:
 * @mio: A #MIO object
 * @offset: Offset of the new place, from @whence
 * @whence: Move origin. SEEK_SET moves relative to the start of the stream,
 *          SEEK_CUR from the current position and SEEK_SET from the end of the
 *          stream.
 * 
 * Sets the curosr position on a #MIO stream. This functions behaves the same as
 * fseek(). See also mio_tell() and mio_setpos().
 * 
 * Returns: 0 on success, -1 otherwise, in which case errno should be set to
 *          indicate the error.
 */
/* FIXME: should we support seeking out of bounds like lseek() seems to do? */
gint
mio_seek (MIO  *mio,
          glong offset,
          gint  whence)
{
  gint rv = -1;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      switch (whence) {
        case SEEK_SET:
          if (offset < 0 || (gsize)offset > mio->impl.mem.size) {
            errno = EINVAL;
          } else {
            mio->impl.mem.pos = offset;
            rv = 0;
          }
          break;
        
        case SEEK_CUR:
          if ((offset < 0 && (gsize)-offset > mio->impl.mem.pos) ||
              mio->impl.mem.pos + offset > mio->impl.mem.size) {
            errno = EINVAL;
          } else {
            mio->impl.mem.pos += offset;
            rv = 0;
          }
          break;
        
        case SEEK_END:
          if (offset > 0 || (gsize)-offset > mio->impl.mem.size) {
            errno = EINVAL;
          } else {
            mio->impl.mem.pos = mio->impl.mem.size - (gsize)-offset;
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
      break;
    
    case MIO_TYPE_FILE:
      rv = fseek (mio->impl.file.fp, offset, whence);
      break;
  }
  
  return rv;
}

/**
 * mio_tell:
 * @mio: A #MIO object
 * 
 * Gets the current cursor position of a #MIO stream. This function behaves the
 * same as ftell().
 * 
 * Returns: The current offset from the start of the stream, or -1 or error, in
 *          which case errno is set to indicate the error.
 */
glong
mio_tell (MIO *mio)
{
  glong rv = -1;
  
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      rv = mio->impl.mem.pos;
      break;
    
    case MIO_TYPE_FILE:
      rv = ftell (mio->impl.file.fp);
      break;
  }
  
  return rv;
}

/**
 * mio_rewind:
 * @mio: A #MIO object
 * 
 * Resets the cursor position to 0, and also the end-of-stream and the error
 * indicators of a #MIO stream.
 * See also mio_seek() and mio_clearerr().
 */
void
mio_rewind (MIO *mio)
{
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      mio->impl.mem.pos = 0;
      mio->impl.mem.ungetch = EOF;
      mio->impl.mem.eof = FALSE;
      mio->impl.mem.error = FALSE;
      break;
    
    case MIO_TYPE_FILE:
      rewind (mio->impl.file.fp);
      break;
  }
}

/**
 * mio_getpos:
 * @mio: A #MIO stream
 * @pos: A #MIOPos object to fill-in
 * 
 * Stores the current position (and maybe other informations about the stream
 * state) of a #MIO stream in order to restore it later with mio_setpos(). This
 * function behaves the same as fgetpos().
 * 
 * Returns: 0 on success, -1 otherwise, in which case errno is set to indicate
 *          the error.
 */
gint
mio_getpos (MIO    *mio,
            MIOPos *pos)
{
  gint rv = -1;
  
  pos->type = mio->type;
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (mio->impl.mem.pos == (gsize)-1) {
        /* this happens if ungetc() was called at the start of the stream */
        #ifdef EIO
        errno = EIO;
        #endif
      } else {
        pos->impl.mem = mio->impl.mem.pos;
        rv = 0;
      }
      break;
    
    case MIO_TYPE_FILE:
      rv = fgetpos (mio->impl.file.fp, &pos->impl.file);
      break;
  }
  #ifdef MIO_DEBUG
  if (rv != -1) {
    pos->tag = mio;
  }
  #endif /* MIO_DEBUG */
  
  return rv;
}

/**
 * mio_setpos:
 * @mio: A #MIO object
 * @pos: A #MIOPos object filled-in by a previous call of mio_getpos() on the
 *       same stream
 * 
 * Restores the position and state indicators of a #MIO stream previously saved
 * by mio_getpos().
 * 
 * <warning><para>The #MIOPos object must have been initialized by a previous
 * call to mio_getpos() on the same stream.</para></warning>
 * 
 * Returns: 0 on success, -1 otherwise, in which case errno is set to indicate
 *          the error.
 */
gint
mio_setpos (MIO    *mio,
            MIOPos *pos)
{
  gint rv = -1;
  
  #ifdef MIO_DEBUG
  if (pos->tag != mio) {
    g_critical ("mio_setpos((MIO*)%p, (MIOPos*)%p): "
                "Given MIOPos was not set by a previous call to mio_getpos() "
                "on the same MIO object, which means there is a bug in "
                "someone's code.",
                (void *)mio, (void *)pos);
    errno = EINVAL;
    return -1;
  }
  #endif /* MIO_DEBUG */
  switch (mio->type) {
    case MIO_TYPE_MEMORY:
      if (pos->impl.mem > mio->impl.mem.size) {
        errno = EINVAL;
      } else {
        mio->impl.mem.ungetch = EOF;
        mio->impl.mem.pos = pos->impl.mem;
        rv = 0;
      }
      break;
    
    case MIO_TYPE_FILE:
      rv = fsetpos (mio->impl.file.fp, &pos->impl.file);
      break;
  }
  
  return rv;
}
