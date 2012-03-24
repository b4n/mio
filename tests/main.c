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

#include <glib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "mio/mio.h"

#define TEST_FILE_R "test.input"
#define TEST_FILE_W "test.output"


static gboolean
create_input_file (const gchar *filename)
{
  FILE     *fpin;
  gboolean  rv = FALSE;
  
  fpin = fopen ("/dev/urandom", "r");
  if (! fpin) {
    /* if we can't open /dev/urandom, try to read ourselves */
    fpin = fopen (__FILE__, "r");
  }
  if (! fpin) {
    g_critical ("Failed to open input file: %s", g_strerror (errno));
  } else {
    FILE *fpout;
    
    fpout = fopen (filename, "w+");
    if (! fpout) {
      g_critical ("Failed to open output file: %s", g_strerror (errno));
    } else {
      gchar  *buf;
      gsize   n_to_read;
      gint    i;
      
      n_to_read = (gsize)g_random_int_range (1, 1024);
      buf = g_malloc (n_to_read * sizeof *buf);
      
      rv = TRUE;
      for (i = 0; rv && i < g_random_int_range (1, 4); i++) {
        gsize n_read;
        
        n_read = fread (buf, sizeof *buf, n_to_read, fpin);
        if (fwrite (buf, sizeof *buf, n_read, fpout) != n_read) {
          g_critical ("Failed to write %lu bytes of data: %s",
                      n_read, g_strerror (errno));
          rv = FALSE;
        }
        if (n_read < n_to_read) {
          break;
        }
      }
      g_free (buf);
      fclose (fpout);
    }
    fclose (fpin);
  }
  
  return rv;
}

static gboolean
create_output_file (const gchar *filename)
{
  FILE *fp;
  
  fp = fopen (filename, "w");
  if (! fp) {
    return FALSE;
  }
  fclose (fp);
  
  return TRUE;
}

static MIO *
test_mio_mem_new_from_file (const gchar *file,
                            gboolean     rw)
{
  MIO    *mio = NULL;
  gchar  *contents;
  gsize   length;
  GError *err = NULL;
  
  if (! g_file_get_contents (file, &contents, &length, &err)) {
    g_critical ("Failed to load file: %s", err->message);
    g_error_free (err);
  } else {
    mio = mio_new_memory ((guchar *)contents, length,
                          rw ? g_try_realloc : NULL, g_free);
  }
  
  return mio;
}

/* note: this function might change the cursor position */
static void
test_mio_dump (MIO *mio)
{
  if (g_test_verbose ()) {
    switch (mio->type) {
      case MIO_TYPE_MEMORY: {
        gsize i;
        
        fprintf (stderr, "---[ memory dump start ]---\n");
        for (i = 0; i < mio->impl.mem.size; i++) {
          fprintf (stderr, "%.2x ", mio->impl.mem.buf[i]);
          if ((i % 8) == 0) {
            fprintf (stderr, "\n");
          }
        }
        fprintf (stderr, "\n----------[ end ]----------\n");
        break;
      }
      
      case MIO_TYPE_FILE: {
        glong length;
        glong i;
        
        mio_seek (mio, 0, SEEK_END);
        length = mio_tell (mio);
        mio_rewind (mio);
        fprintf (stderr, "---[ file dump start ]---\n");
        for (i = 0; i < length; i++) {
          fprintf (stderr, "%.2x ", mio_getc (mio));
          if ((i % 8) == 0) {
            fprintf (stderr, "\n");
          }
        }
        fprintf (stderr, "\n----------[ end ]----------\n");
        break;
      }
    }
  }
}

static gint
test_miocmp (MIO *a,
             MIO *b)
{
  guchar  pa[64], pb[64];
  gsize   na = 0, nb = 0;
  gint    rv = 0;
  glong   posa, posb;
  
  if ((posa = mio_tell (a)) < 0) return G_MAXINT;
  if ((posb = mio_tell (b)) < 0) return G_MININT;
  mio_rewind (a);
  mio_rewind (b);
  while (rv == 0) {
    na += mio_read (a, pa, 1, 64);
    nb += mio_read (b, pb, 1, 64);
    if (na != nb) {
      rv = (gint)(na - nb);
    } else if (na != 64) {
      rv = mio_error (a) - mio_error (b);
      break;
    } else {
      rv = memcmp (pa, pb, na);
    }
  }
  if (mio_seek (a, posa, SEEK_SET) != 0) return G_MAXINT;
  if (mio_seek (b, posb, SEEK_SET) != 0) return G_MININT;
  
  return rv;
}

static void
test_random_mem (void  *ptr,
                 gsize  n)
{
  gsize i;
  
  for (i = 0; i < n; i++) {
    ((guchar *)ptr)[i] = (guchar)g_random_int_range (0, 255);
  }
}

static void verbose (const gchar *fmt, ...) G_GNUC_PRINTF (1, 2);
static void
verbose (const gchar *fmt,
         ...)
{
  if (g_test_verbose ()) {
    va_list ap;
    
    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
  }
}


#define range(var, start, end, step) var = start; var < end; var += step
#define range_loop(var, start, end, step) for (range(var, start, end, step))
#define loop(var, n) range_loop(var, 0, n, 1)


#define TEST_DECLARE_VAR(type, name, value) \
  type name##_m = value; \
  type name##_f = value;
#define TEST_DECLARE_ARRAY(type, name, size, init) \
  type name##_m[size] = init; \
  type name##_f[size] = init;


#define TEST_CREATE_MIO(var, file, rw)              \
  var##_m = test_mio_mem_new_from_file (file, rw);  \
  var##_f = mio_new_file (file, rw ? "r+b" : "rb"); \
  g_assert (var##_m != NULL && var##_f != NULL);    \
  errno = 0; /* WAT, it is ENOENT - no idea, IIRC GLib throws it */
#define TEST_DESTROY_MIO(var) \
  mio##_m = mio##_f = (mio_free (var##_m), mio_free (var##_f), NULL);


#define assert_cmpmio(mio1, cmp, mio2)                                         \
  do { MIO *__mio1 = (mio1), *__mio2 = (mio2);                                 \
       gint __result = test_miocmp (__mio1, __mio2);                           \
    if (__result cmp 0) ; else                                                 \
      /*test_mio_dump (__mio1), test_mio_dump (__mio2), */                     \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           g_strdup_printf ("(MIO*)%p " #cmp " (MIO*)%p, %d",  \
                                            (void *)__mio1, (void *)__mio2,    \
                                            __result));                        \
  } while (0)

#define assert_cmpptr(p1, cmp, p2, n)                                          \
  do { void *__p1 = (p1), *__p2 = (p2);                                        \
    if (memcmp (__p1, __p2, (n)) cmp 0) ; else                                 \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           g_strdup_printf ("\"%.*s\" " #cmp " \"%.*s\"",      \
                                            (gint)(n), (char *)__p1,           \
                                            (gint)(n), (char *)__p2));         \
  } while (0)

#define assert_errno(errno, cmp, val)                                          \
  do { gint __errnum = (errno), __val = (val);                                 \
    if (__val == -1 || __errnum cmp __val) ; else                              \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           g_strdup_printf ("Unexpected errno %d (%s)",        \
                                            __errnum, g_strerror (__errnum))); \
    errno = 0;                                                                 \
  } while (0)


/* read */
#define READ(ret, mio, ptr, size, nmemb)                                       \
  (ret = mio_read (mio, ptr, size, nmemb),                                     \
   ((ret < nmemb)                                                              \
    ? ((mio_error (mio))                                                       \
       ? verbose ("mio_read() failed (read %"G_GSIZE_FORMAT")\n", ret)         \
       : verbose ("mio_read() succeeded (read %"G_GSIZE_FORMAT", "             \
                  "queried %"G_GSIZE_FORMAT")\n", ret, nmemb))                 \
    : verbose ("mio_read() succeeded (read %"G_GSIZE_FORMAT")\n", ret)),       \
   ret)
#define TEST_READ(ret, mio, ptr, size, nmemb, ex_err) \
  ret##_f = READ(ret##_f, mio##_f, ptr##_f, size##_f, nmemb##_f); \
  assert_errno (errno, ==, ex_err);                               \
  ret##_m = READ(ret##_m, mio##_m, ptr##_m, size##_m, nmemb##_m); \
  assert_errno (errno, ==, ex_err);                               \
  g_assert_cmpuint (ret##_f, ==, ret##_m);                        \
  assert_cmpptr (ptr##_m, ==, ptr##_f, ret##_m * size##_m)

/* getc */
#define GETC(ret, mio)                                                         \
  (ret = mio_getc (mio),                                                       \
   ((ret == EOF)                                                               \
    ? verbose (((mio_eof (mio))                                                \
                ? "mio_getc() reached EOF\n"                                   \
                : "mio_getc() failed\n"))                                      \
    : verbose ("mio_getc() succeeded (c = %d ('%c'))\n", ret, ret)),           \
   ret)
#define TEST_GETC(ret, mio, ex_err)         \
  ret##_f = GETC(ret##_f, mio##_f);         \
  assert_errno (errno, ==, ex_err);         \
  ret##_m = GETC(ret##_m, mio##_m);         \
  assert_errno (errno, ==, ex_err);         \
  g_assert_cmpint (ret##_f, ==, ret##_m)

/* gets */
#define GETS(ret, mio, s, size)                                                \
  (ret = mio_gets (mio, s, size),                                              \
   ((ret != s)                                                                 \
    ? verbose (((mio_error (mio))                                              \
                ? "mio_gets() failed\n"                                        \
                : "mio_gets() reached EOF\n"))                                 \
    : verbose ("mio_gets() succeeded (s = \"%s\")\n", ret)),                   \
   ret)
#define TEST_GETS(ret, mio, s, size, ex_err)           \
  ret##_f = GETS(ret##_f, mio##_f, s##_f, size##_f);   \
  assert_errno (errno, ==, ex_err);                    \
  ret##_m = GETS(ret##_m, mio##_m, s##_m, size##_m);   \
  assert_errno (errno, ==, ex_err);                    \
  g_assert_cmpstr (ret##_f, ==, ret##_m)

/* ungetc */
#define UNGETC(ret, mio, c)                                                    \
  (ret = mio_ungetc (mio, c),                                                  \
   ((ret == EOF && c != EOF)                                                   \
    ? verbose ("mio_ungetc(%d ('%c')) failed\n", c, c)                         \
    : verbose ("mio_ungetc(%d ('%c')) succeeded\n", c, c)),                    \
   ret)
#define TEST_UNGETC(ret, mio, c, ex_err)    \
  ret##_f = UNGETC(ret##_f, mio##_f, c);    \
  assert_errno (errno, ==, ex_err);         \
  ret##_m = UNGETC(ret##_m, mio##_m, c);    \
  assert_errno (errno, ==, ex_err);         \
  g_assert_cmpint (ret##_f, ==, ret##_m)

/* write */
#define WRITE(ret, mio, ptr, size, nmemb)                                      \
  (ret = mio_write (mio, ptr, size, nmemb),                                    \
   ((ret < nmemb)                                                              \
    ? verbose ("mio_write() failed (wrote %"G_GSIZE_FORMAT", "                 \
               "queried %"G_GSIZE_FORMAT")\n", ret, nmemb)                     \
    : verbose ("mio_write() succeeded (wrote %"G_GSIZE_FORMAT")\n",            \
               ret)),                                                          \
   ret)
#define TEST_WRITE(ret, mio, ptr, size, nmemb, ex_err)   \
  ret##_f = WRITE(ret##_f, mio##_f, ptr, size, nmemb);   \
  assert_errno (errno, ==, ex_err);                      \
  ret##_m = WRITE(ret##_m, mio##_m, ptr, size, nmemb);   \
  assert_errno (errno, ==, ex_err);                      \
  g_assert_cmpuint (ret##_f, ==, ret##_m)

/* putc */
#define PUTC(ret, mio, c)                                                      \
  (ret = mio_putc (mio, c),                                                    \
   ((ret == EOF)                                                               \
    ? verbose ("mio_putc(%d ('%c')) failed\n", c, c)                           \
    : verbose ("mio_putc(%d ('%c')) succeeded\n", c, c)),                      \
   ret)
#define TEST_PUTC(ret, mio, c, ex_err)    \
  ret##_f = PUTC(ret##_f, mio##_f, c);    \
  assert_errno (errno, ==, ex_err);       \
  ret##_m = PUTC(ret##_m, mio##_m, c);    \
  assert_errno (errno, ==, ex_err);       \
  g_assert_cmpint (ret##_f, ==, ret##_m)

/* puts */
#define PUTS(ret, mio, s)                                                      \
  (ret = mio_puts (mio, s),                                                    \
   ((ret == EOF)                                                               \
    ? verbose ("mio_puts(\"%s\") failed\n", s)                                 \
    : verbose ("mio_puts(\"%s\") succeeded\n", s)),                            \
   ret)
#define TEST_PUTS(ret, mio, s, ex_err)    \
  ret##_f = PUTS(ret##_f, mio##_f, s);    \
  assert_errno (errno, ==, ex_err);       \
  ret##_m = PUTS(ret##_m, mio##_m, s);    \
  assert_errno (errno, ==, ex_err);       \
  g_assert_cmpint (ret##_f, ==, ret##_m)

/* seek */
#define SEEK(ret, mio, off, wh)                                                \
  (ret = mio_seek (mio, off, wh),                                              \
   ((ret != 0)                                                                 \
    ? verbose ("mio_seek(%ld, %d) failed: %s\n",                               \
               (glong)off, wh, g_strerror (errno))                             \
    : verbose ("mio_seek(%ld, %d) succeeded\n", (glong)off, wh)),              \
   ret)
#define TEST_SEEK(ret, mio, off, wh, ex_err)    \
  ret##_f = SEEK(ret##_f, mio##_f, off, wh);    \
  assert_errno (errno, ==, ex_err);             \
  ret##_m = SEEK(ret##_m, mio##_m, off, wh);    \
  assert_errno (errno, ==, ex_err);             \
  g_assert_cmpint (ret##_f, ==, ret##_m)

/* tell */
#define TELL(ret, mio)                                                         \
  (ret = mio_tell (mio),                                                       \
   ((ret == -1)                                                                \
    ? verbose ("mio_tell() failed: %s\n", g_strerror (errno))                  \
    : verbose ("mio_tell() succeeded (pos = %ld)\n", ret)),                    \
   ret)
#define TEST_TELL(ret, mio, ex_err)     \
  ret##_f = TELL(ret##_f, mio##_f);     \
  assert_errno (errno, ==, ex_err);     \
  ret##_m = TELL(ret##_m, mio##_m);     \
  assert_errno (errno, ==, ex_err);     \
  g_assert_cmpint (ret##_f, ==, ret##_m)

/* rewind */
#define REWIND(mio)                                                            \
  (mio_rewind (mio), verbose ("mio_rewind()\n"))
#define TEST_REWIND(mio, ex_err)    \
  REWIND (mio##_f);                 \
  assert_errno (errno, ==, ex_err); \
  REWIND (mio##_m);                 \
  assert_errno (errno, ==, ex_err)

/* getpos */
#define GETPOS(ret, mio, pos)                                                  \
  (ret = mio_getpos (mio, pos),                                                \
   ((ret != 0)                                                                 \
    ? verbose ("mio_getpos() failed: %s\n", g_strerror (errno))                \
    : verbose ("mio_getpos() succeeded\n")),                                   \
   ret)
#define TEST_GETPOS(ret, mio, pos, ex_err)        \
  ret##_f = GETPOS(ret##_f, mio##_f, pos##_f);    \
  assert_errno (errno, ==, ex_err);               \
  ret##_m = GETPOS(ret##_m, mio##_m, pos##_m);    \
  assert_errno (errno, ==, ex_err);               \
  g_assert_cmpint (ret##_f, ==, ret##_m)

/* setpos */
#define SETPOS(ret, mio, pos)                                                  \
  (ret = mio_setpos (mio, pos),                                                \
   ((ret != 0)                                                                 \
    ? verbose ("mio_setpos() failed: %s\n", g_strerror (errno))                \
    : verbose ("mio_setpos() succeeded\n")),                                   \
   ret)
#define TEST_SETPOS(ret, mio, pos, ex_err)        \
  ret##_f = SETPOS(ret##_f, mio##_f, pos##_f);    \
  assert_errno (errno, ==, ex_err);               \
  ret##_m = SETPOS(ret##_m, mio##_m, pos##_m);    \
  assert_errno (errno, ==, ex_err);               \
  g_assert_cmpint (ret##_f, ==, ret##_m)

/* eof */
#define EOF_(ret, mio)                                                         \
  (ret = mio_eof (mio),                                                        \
   verbose ("mio_eof() %s\n", ret ? "reached EOF" : "not reached EOF"),        \
   ret)
#define TEST_EOF(ret, mio)              \
  ret##_f = EOF_(ret##_f, mio##_f);     \
  ret##_m = EOF_(ret##_m, mio##_m);     \
  g_assert_cmpint (ret##_f, ==, ret##_m)
  
/* error */
#define ERROR(ret, mio)                                                        \
  (ret = mio_error (mio),                                                      \
   verbose ("mio_error() reported %s\n", ret ? "an error" : "no error"),       \
   ret)
#define TEST_ERROR(ret, mio)             \
  ret##_f = ERROR(ret##_f, mio##_f);     \
  ret##_m = ERROR(ret##_m, mio##_m);     \
  g_assert_cmpint (ret##_f, ==, ret##_m)
  
/* error */
#define CLEARERR(mio)                                                          \
  (mio_clearerr (mio), verbose ("mio_clearerr()\n"))
#define TEST_CLEARERR(mio)  \
  (CLEARERR(mio##_f),       \
   CLEARERR(mio##_m))


static void
test_read_read (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_ARRAY (gchar, ptr, 255, {0})
  TEST_DECLARE_VAR (gsize, n, 0)
  TEST_DECLARE_VAR (gint, c, 0)
  TEST_DECLARE_VAR (gsize, size, 1)
  TEST_DECLARE_VAR (gsize, nmemb, 255)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  loop (i, 3) {
    TEST_READ (n, mio, ptr, size, nmemb, 0);
  }
  TEST_UNGETC (c, mio, 'X', 0);
  loop (i, 3) {
    TEST_READ (n, mio, ptr, size, nmemb, 0);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_read_read_partial (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_ARRAY (gchar, ptr, 5, {0})
  TEST_DECLARE_VAR (gsize, size, 2)
  TEST_DECLARE_VAR (gsize, nmemb, 2)
  TEST_DECLARE_VAR (gsize, n, 0)
  TEST_DECLARE_VAR (gint, c, 0)
  TEST_DECLARE_VAR (glong, pos, 0)
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  TEST_SEEK (c, mio, -3, SEEK_END, 0);
  TEST_READ (n, mio, ptr, size, nmemb, 0);
  TEST_TELL (pos, mio, 0);
  
  TEST_SEEK (c, mio, -2, SEEK_END, 0);
  TEST_UNGETC (c, mio, '1', 0);
  TEST_READ (n, mio, ptr, size, nmemb, 0);
  TEST_TELL (pos, mio, 0);
  
  TEST_DESTROY_MIO (mio)
}

static void
test_read_getc (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  loop (i, 3) {
    TEST_GETC (c, mio, 0);
  }
  TEST_UNGETC (c, mio, 'X', 0);
  loop (i, 35) {
    TEST_GETC (c, mio, 0);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_read_gets (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_ARRAY (gchar, s, 255, {0})
  TEST_DECLARE_VAR (gchar*, sr, NULL)
  TEST_DECLARE_VAR (gsize, size, 255)
  TEST_DECLARE_VAR (gint, c, 0)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  loop (i, 3) {
    TEST_GETS (sr, mio, s, size, 0);
  }
  TEST_UNGETC (c, mio, 'X', 0);
  loop (i, 3) {
    TEST_GETS (sr, mio, s, size, 0);
  }
  
  TEST_DESTROY_MIO (mio)
}


static void
test_write_write (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  gchar ptr[255] = {0};
  TEST_DECLARE_ARRAY (gchar, readptr, 255, {0})
  TEST_DECLARE_VAR (gsize, size, 1)
  TEST_DECLARE_VAR (gsize, nmemb, 255)
  TEST_DECLARE_VAR (gsize, n, 0)
  TEST_DECLARE_VAR (gint, c, 0)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_W, TRUE)
  
  assert_cmpmio (mio_m, ==, mio_f);
  
  test_random_mem (ptr, sizeof (*ptr) * sizeof (ptr));
  loop (i, 3) {
    TEST_WRITE (n, mio, ptr, sizeof *ptr, sizeof ptr, 0);
  }
  TEST_SEEK (c, mio, sizeof *ptr * sizeof ptr / 2, SEEK_SET, 0);
  TEST_READ (n, mio, readptr, size, nmemb, 0);
  loop (i, 128) {
    if (i > 64) {
      memset (ptr, i, sizeof (*ptr) * sizeof (ptr));
    }
    TEST_WRITE (n, mio, ptr, sizeof *ptr, sizeof ptr, 0);
  }
  
  assert_cmpmio (mio_m, ==, mio_f);
  
  TEST_DESTROY_MIO (mio)
}

static void
test_write_putc (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  gchar ptr[255] = {0};
  TEST_DECLARE_VAR (gint, c, 0)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_W, TRUE)
  
  test_random_mem (ptr, sizeof (*ptr) * sizeof (ptr));
  loop (i, 3) {
    TEST_PUTC (c, mio, ptr[i], 0);
  }
  TEST_SEEK (c, mio, 1, SEEK_SET, 0);
  loop (i, 128) {
    TEST_PUTC (c, mio, ptr[i], 0);
  }
  TEST_PUTC (c, mio, 4096, 0);
  
  assert_cmpmio (mio_m, ==, mio_f);
  
  TEST_DESTROY_MIO (mio)
}

static void
test_write_puts (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  const gchar *strs[] = {
    "a",
    "bcdef",
    "\025\075\002",
    "hi all",
    ""
  };
  TEST_DECLARE_VAR (gint, c, 0)
  guint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_W, TRUE)
  
  loop (i, (G_N_ELEMENTS (strs) / 2)) {
    TEST_PUTS (c, mio, strs[i], 0);
  }
  TEST_SEEK (c, mio, 1, SEEK_SET, 0);
  loop (i, G_N_ELEMENTS (strs)) {
    TEST_PUTS (c, mio, strs[i], 0);
  }
  TEST_PUTS (c, mio, "\022\074hello\033", 0);
  
  assert_cmpmio (mio_m, ==, mio_f);
  
  TEST_DESTROY_MIO (mio)
}

static void
test_write_printf (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  
  TEST_CREATE_MIO (mio, TEST_FILE_W, TRUE)
  
  c_f = mio_printf (mio_f, "hi! %d %s %ld %p\n", 42, "boy", 84L, (void *)mio_f);
  if (c_f < 0)  verbose ("mio_printf() failed\n");
  else          verbose ("mio_printf() succeeded\n");
  c_m = mio_printf (mio_m, "hi! %d %s %ld %p\n", 42, "boy", 84L, (void *)mio_f);
  if (c_f < 0)  verbose ("mio_printf() failed\n");
  else          verbose ("mio_printf() succeeded\n");
  g_assert_cmpint (c_f, ==, c_m);
  c_f = mio_printf (mio_f, "%.42s %f", "AAAAAAAH\n", 2.854);
  if (c_f < 0)  verbose ("mio_printf() failed\n");
  else          verbose ("mio_printf() succeeded\n");
  c_m = mio_printf (mio_m, "%.42s %f", "AAAAAAAH\n", 2.854);
  if (c_f < 0)  verbose ("mio_printf() failed\n");
  else          verbose ("mio_printf() succeeded\n");
  g_assert_cmpint (c_f, ==, c_m);
  
  assert_cmpmio (mio_m, ==, mio_f);
  
  TEST_DESTROY_MIO (mio)
}


static void
test_pos_tell (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  TEST_DECLARE_VAR (glong, pos, 0)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  loop (i, 3) {
    TEST_TELL (pos, mio, 0);
    TEST_GETC (c, mio, 0);
  }
  TEST_TELL (pos, mio, 0);
  if (pos_f > 0) {
    TEST_UNGETC (c, mio, 'X', 0);
  }
  loop (i, 3) {
    TEST_TELL (pos, mio, 0);
    TEST_GETC (c, mio, 0);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_pos_seek (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  TEST_DECLARE_VAR (glong, pos, 0)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  if (mio_m->impl.mem.size < 7) {
    fprintf (stderr, "** This test needs a streem with more than 6 characters "
                     "because the glibc supports a feature we don't, and that "
                     "is revealed by this test with shorter inputs: "
                     "seeking after the end of the stream.\n");
  } else {
    loop (i, 3) {
      TEST_SEEK (c, mio, (glong)i, SEEK_SET, 0);
      TEST_GETC (c, mio, 0);
      TEST_TELL (pos, mio, 0);
    }
    TEST_UNGETC (c, mio, 'X', 0);
    loop (i, 3) {
      TEST_TELL (pos, mio, 0);
      TEST_SEEK (c, mio, (glong)i, SEEK_CUR, 0);
      TEST_TELL (pos, mio, 0);
      TEST_GETC (c, mio, 0);
    }
    TEST_UNGETC (c, mio, 'X', 0);
    loop (i, 3) {
      TEST_SEEK (c, mio, -1, SEEK_END, 0);
      TEST_TELL (pos, mio, 0);
      TEST_GETC (c, mio, 0);
    }
    TEST_UNGETC (c, mio, 'X', 0);
    loop (i, 3) {
      TEST_SEEK (c, mio, i, SEEK_SET, 0);
      TEST_GETC (c, mio, 0);
      TEST_TELL (pos, mio, 0);
    }
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_pos_rewind (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  TEST_DECLARE_VAR (glong, pos, 0)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  loop (i, 3) {
    TEST_REWIND (mio, 0);
    TEST_TELL (pos, mio, 0);
    TEST_GETC (c, mio, 0);
  }
  TEST_UNGETC (c, mio, 'X', 0);
  loop (i, 3) {
    TEST_REWIND (mio, 0);
    TEST_TELL (pos, mio, 0);
    TEST_GETC (c, mio, 0);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_pos_getpos (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  TEST_DECLARE_VAR (MIOPos, pos, {0})
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  loop (i, 3) {
    TEST_GETPOS (c, mio, &pos, 0);
    TEST_GETC (c, mio, 0);
  }
  if (mio_tell (mio_f) > 0) {
    TEST_UNGETC (c, mio, 'X', 0);
  }
  loop (i, 3) {
    TEST_GETPOS (c, mio, &pos, 0);
    TEST_GETC (c, mio, 0);
  }
  
  loop (i, 3) {
    TEST_SEEK (c, mio, -1, SEEK_END, -1);
    TEST_GETPOS (c, mio, &pos, -1);
    TEST_GETC (c, mio, 0);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_pos_setpos (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  TEST_DECLARE_VAR (MIOPos, pos, {0})
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  loop (i, 3) {
    TEST_GETPOS (c, mio, &pos, 0);
    TEST_GETC (c, mio, 0);
    TEST_SETPOS (c, mio, &pos, 0);
  }
  /* don't ungetc() at start because C99 standard defines this as an
   * "obsolescent feature", and the GNU libc does strange things with this */
  if (mio_m->impl.mem.size > 0) { /* see test_pos_seek */
    TEST_SEEK (c, mio, 1, SEEK_SET, -1);
    if (c_f > 0) {
      TEST_UNGETC (c, mio, 'X', 0);
    }
  }
  
  loop (i, 3) {
    TEST_GETPOS (c, mio, &pos, 0);
    TEST_GETC (c, mio, 0);
    TEST_SETPOS (c, mio, &pos, 0);
  }
  
  TEST_DESTROY_MIO (mio)
}


static void
test_error_eof (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  TEST_DECLARE_VAR (glong, pos, 0)
  TEST_DECLARE_ARRAY (gchar, ptr, 255, {0})
  TEST_DECLARE_ARRAY (gchar, s, 255, {0})
  TEST_DECLARE_VAR (gsize, s_size, 255)
  TEST_DECLARE_VAR (gchar*, sr, NULL)
  TEST_DECLARE_VAR (gsize, n, 0)
  TEST_DECLARE_VAR (gsize, size, sizeof *ptr_m)
  TEST_DECLARE_VAR (gsize, nmemb, sizeof ptr_m)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  loop (i, 3) {
    TEST_SEEK (pos, mio, -i, SEEK_END, -1);
    TEST_GETC (c, mio, 0);
    TEST_EOF (c, mio);
  }
  TEST_UNGETC (c, mio, 'X', 0);
  loop (i, 3) {
    TEST_SEEK (pos, mio, -i, SEEK_END, -1);
    TEST_GETC (c, mio, 0);
    TEST_EOF (c, mio);
  }
  TEST_SEEK (pos, mio, 0, SEEK_END, 0);
  TEST_EOF (c, mio);
  /* read() checks */
  TEST_READ (n, mio, ptr, size, nmemb, 0);
  TEST_EOF (c, mio);
  TEST_SEEK (pos, mio, 0, SEEK_SET, 0);
  TEST_READ (n, mio, ptr, size, nmemb, 0);
  TEST_EOF (c, mio);
  /* gets() checks */
  TEST_GETS (sr, mio, s, s_size, 0);
  TEST_EOF (c, mio);
  TEST_SEEK (pos, mio, 0, SEEK_END, 0);
  TEST_GETS (sr, mio, s, s_size, 0);
  TEST_EOF (c, mio);
  
  loop (i, 128) {
    TEST_GETC (c, mio, 0);
    TEST_EOF (c, mio);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_error_error (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  TEST_ERROR (c, mio);
  loop (i, 128) {
    TEST_GETC (c, mio, 0);
    TEST_ERROR (c, mio);
  }
  TEST_SEEK (c, mio, -2, SEEK_SET, EINVAL);
  TEST_ERROR (c, mio);
  
  TEST_DESTROY_MIO (mio);
}

static void
test_error_clearerr (void)
{
  TEST_DECLARE_VAR (MIO*, mio, NULL)
  TEST_DECLARE_VAR (gint, c, 0)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE_R, FALSE)
  
  TEST_ERROR (c, mio);
  TEST_CLEARERR (mio);
  TEST_ERROR (c, mio);
  loop (i, 128) {
    TEST_ERROR (c, mio);
    TEST_GETC (c, mio, 0);
    TEST_ERROR (c, mio);
    TEST_CLEARERR (mio);
    TEST_ERROR (c, mio);
  }
  TEST_ERROR (c, mio);
  TEST_SEEK (c, mio, -2, SEEK_SET, EINVAL);
  TEST_ERROR (c, mio);
  TEST_CLEARERR (mio);
  TEST_ERROR (c, mio);
  
  TEST_DESTROY_MIO (mio);
}



#define ADD_TEST_FUNC(section, name) \
  g_test_add_func ("/"#section"/"#name, test_##section##_##name)

int
main (int     argc,
      char  **argv)
{
  g_test_init (&argc, &argv, NULL);
  
  create_input_file (TEST_FILE_R);
  create_output_file (TEST_FILE_W);
  
  ADD_TEST_FUNC (read, read);
  ADD_TEST_FUNC (read, read_partial);
  ADD_TEST_FUNC (read, getc);
  ADD_TEST_FUNC (read, gets);
  ADD_TEST_FUNC (write, write);
  ADD_TEST_FUNC (write, putc);
  ADD_TEST_FUNC (write, puts);
  ADD_TEST_FUNC (write, printf);
  ADD_TEST_FUNC (pos, tell);
  ADD_TEST_FUNC (pos, seek);
  ADD_TEST_FUNC (pos, rewind);
  ADD_TEST_FUNC (pos, getpos);
  ADD_TEST_FUNC (pos, setpos);
  ADD_TEST_FUNC (error, eof);
  ADD_TEST_FUNC (error, error);
  ADD_TEST_FUNC (error, clearerr);
  
  g_test_run ();
  
  remove (TEST_FILE_W);
  remove (TEST_FILE_R);
  
  return 0;
}
