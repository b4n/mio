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

#include <glib.h>
#include <string.h>
#include <errno.h>
#include "mio/mio.h"

#define TEST_FILE "test.input"


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


#define range(var, start, end, step) var = start; var < end; var += step
#define range_loop(var, start, end, step) for (range(var, start, end, step))
#define loop(var, n) range_loop(var, 0, n, 1)


#define TEST_DECLARE_VAR(type, name) \
  type name##_m; \
  type name##_f;
#define TEST_DECLARE_ARRAY(type, name, size) \
  type name##_m[size]; \
  type name##_f[size];

#define TEST_CREATE_MIO(var, file, rw)              \
  var##_m = test_mio_mem_new_from_file (file, rw);  \
  var##_f = mio_new_file (file, rw ? "r+b" : "rb"); \
  g_assert (var##_m != NULL && var##_f != NULL);    \
  errno = 0; /* WAT, it is ENOENT - no idea, IIRC GLib throws it */
#define TEST_DESTROY_MIO(var) \
  mio##_m = mio##_f = (mio_free (var##_m), mio_free (var##_f), NULL);

#define TEST_ACTION_0(ret_var, func, mio_var, ex_err)                          \
  errno = 0;                                                                   \
  ret_var##_m = func (mio_var##_m);                                            \
  assert_errno (errno, ==, ex_err);                                            \
  ret_var##_f = func (mio_var##_f);                                            \
  assert_errno (errno, ==, ex_err);
#define TEST_ACTION_1(ret_var, func, mio_var, p1, ex_err)                      \
  errno = 0;                                                                   \
  ret_var##_m = func (mio_var##_m, p1);                                        \
  assert_errno (errno, ==, ex_err);                                            \
  ret_var##_f = func (mio_var##_f, p1);                                        \
  assert_errno (errno, ==, ex_err);
#define TEST_ACTION_2(ret_var, func, mio_var, p1, p2, ex_err)                  \
  errno = 0;                                                                   \
  ret_var##_m = func (mio_var##_m, p1, p2);                                    \
  assert_errno (errno, ==, ex_err);                                            \
  ret_var##_f = func (mio_var##_f, p1, p2);                                    \
  assert_errno (errno, ==, ex_err);


#define assert_cmpptr(p1, cmp, p2, n)                                          \
  do { void *__p1 = (p1), *__p2 = (p2);                                        \
    if (memcmp (__p1, __p2, n) cmp 0) ; else                                   \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           g_strdup_printf ("\"%.*s\" " #cmp " \"%.*s\"",      \
                                            n, (char *)__p1, n, (char *)__p2));\
  } while (0)

#define assert_errno(errno, cmp, val)                                          \
  do { gint __errnum = (errno), __val = (val);                                 \
    if (__errnum cmp __val) ; else                                             \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           g_strerror (__errnum));                             \
  } while (0)



static void
test_read_read (void)
{
  TEST_DECLARE_VAR (MIO*, mio)
  TEST_DECLARE_ARRAY (gchar, ptr, 255)
  TEST_DECLARE_VAR (gsize, n)
  TEST_DECLARE_VAR (gint, c)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE, FALSE)
  
  loop (i, 3) {
    n_m = mio_read (mio_m, ptr_m, sizeof (*ptr_m), sizeof (ptr_m));
    assert_errno (errno, ==, 0);
    n_f = mio_read (mio_f, ptr_f, sizeof (*ptr_f), sizeof (ptr_f));
    assert_errno (errno, ==, 0);
    g_assert_cmpuint (n_m, ==, n_f);
    assert_cmpptr (ptr_m, ==, ptr_f, n_m);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  loop (i, 3) {
    n_m = mio_read (mio_m, ptr_m, sizeof (*ptr_m), sizeof (ptr_m));
    assert_errno (errno, ==, 0);
    n_f = mio_read (mio_f, ptr_f, sizeof (*ptr_f), sizeof (ptr_f));
    assert_errno (errno, ==, 0);
    g_assert_cmpuint (n_m, ==, n_f);
    assert_cmpptr (ptr_m, ==, ptr_f, n_m);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_read_getc (void)
{
  TEST_DECLARE_VAR (MIO*, mio)
  TEST_DECLARE_VAR (gint, c)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE, FALSE)
  
  loop (i, 3) {
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  loop (i, 3) {
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_read_gets (void)
{
  TEST_DECLARE_VAR (MIO*, mio)
  TEST_DECLARE_ARRAY (gchar, s, 255)
  TEST_DECLARE_VAR (gchar*, sr)
  TEST_DECLARE_VAR (gint, c)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE, FALSE)
  
  loop (i, 3) {
    sr_m = mio_gets (mio_m, s_m, 255);
    assert_errno (errno, ==, 0);
    sr_f = mio_gets (mio_f, s_f, 255);
    assert_errno (errno, ==, 0);
    g_assert_cmpstr (sr_m, ==, sr_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  loop (i, 3) {
    sr_m = mio_gets (mio_m, s_m, 255);
    assert_errno (errno, ==, 0);
    sr_f = mio_gets (mio_f, s_f, 255);
    assert_errno (errno, ==, 0);
    g_assert_cmpstr (sr_m, ==, sr_f);
  }
  
  TEST_DESTROY_MIO (mio)
}


static void
test_pos_tell (void)
{
  TEST_DECLARE_VAR (MIO*, mio)
  TEST_DECLARE_VAR (gint, c)
  TEST_DECLARE_VAR (glong, pos)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE, FALSE)
  
  loop (i, 3) {
    TEST_ACTION_0 (pos, mio_tell, mio, 0)
    g_assert_cmpint (pos_m, ==, pos_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  loop (i, 3) {
    TEST_ACTION_0 (pos, mio_tell, mio, 0)
    g_assert_cmpint (pos_m, ==, pos_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_pos_seek (void)
{
  TEST_DECLARE_VAR (MIO*, mio)
  TEST_DECLARE_VAR (gint, c)
  TEST_DECLARE_VAR (glong, pos)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE, FALSE)
  
  loop (i, 3) {
    TEST_ACTION_2 (c, mio_seek, mio, i, SEEK_SET, 0)
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (pos, mio_tell, mio, 0)
    g_assert_cmpint (pos_m, ==, pos_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  loop (i, 3) {
    TEST_ACTION_2 (c, mio_seek, mio, i, SEEK_CUR, 0)
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (pos, mio_tell, mio, 0)
    g_assert_cmpint (pos_m, ==, pos_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  loop (i, 3) {
    TEST_ACTION_2 (c, mio_seek, mio, -i, SEEK_END, 0)
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (pos, mio_tell, mio, 0)
    g_assert_cmpint (pos_m, ==, pos_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  loop (i, 3) {
    TEST_ACTION_2 (c, mio_seek, mio, i, SEEK_SET, 0)
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (pos, mio_tell, mio, 0)
    g_assert_cmpint (pos_m, ==, pos_f);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_pos_rewind (void)
{
  TEST_DECLARE_VAR (MIO*, mio)
  TEST_DECLARE_VAR (gint, c)
  TEST_DECLARE_VAR (glong, pos)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE, FALSE)
  
  loop (i, 3) {
    mio_rewind (mio_m);
    assert_errno (errno, ==, 0);
    mio_rewind (mio_f);
    assert_errno (errno, ==, 0);
    TEST_ACTION_0 (pos, mio_tell, mio, 0)
    g_assert_cmpint (pos_m, ==, pos_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  loop (i, 3) {
    mio_rewind (mio_m);
    assert_errno (errno, ==, 0);
    mio_rewind (mio_f);
    assert_errno (errno, ==, 0);
    TEST_ACTION_0 (pos, mio_tell, mio, 0)
    g_assert_cmpint (pos_m, ==, pos_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_pos_getpos (void)
{
  TEST_DECLARE_VAR (MIO*, mio)
  TEST_DECLARE_VAR (gint, c)
  TEST_DECLARE_VAR (MIOPos, pos)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE, FALSE)
  
  loop (i, 3) {
    c_m = mio_getpos (mio_m, &pos_m);
    assert_errno (errno, ==, 0);
    c_f = mio_getpos (mio_f, &pos_f);
    assert_errno (errno, ==, 0);
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  loop (i, 3) {
    c_m = mio_getpos (mio_m, &pos_m);
    assert_errno (errno, ==, 0);
    c_f = mio_getpos (mio_f, &pos_f);
    assert_errno (errno, ==, 0);
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  
  loop (i, 3) {
    TEST_ACTION_2 (c, mio_seek, mio, -i, SEEK_END, 0)
    c_m = mio_getpos (mio_m, &pos_m);
    assert_errno (errno, ==, 0);
    c_f = mio_getpos (mio_f, &pos_f);
    assert_errno (errno, ==, 0);
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
  }
  
  TEST_DESTROY_MIO (mio)
}

static void
test_pos_setpos (void)
{
  TEST_DECLARE_VAR (MIO*, mio)
  TEST_DECLARE_VAR (gint, c)
  TEST_DECLARE_VAR (MIOPos, pos)
  gint i;
  
  TEST_CREATE_MIO (mio, TEST_FILE, FALSE)
  
  loop (i, 3) {
    c_m = mio_getpos (mio_m, &pos_m);
    assert_errno (errno, ==, 0);
    c_f = mio_getpos (mio_f, &pos_f);
    assert_errno (errno, ==, 0);
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
    c_m = mio_setpos (mio_m, &pos_m);
    assert_errno (errno, ==, 0);
    c_f = mio_setpos (mio_f, &pos_f);
    assert_errno (errno, ==, 0);
    g_assert_cmpint (c_m, ==, c_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X', 0)
  g_assert_cmpint (c_m, ==, c_f);
  /* those should fail because current posititon is -1 (because of the
   * ungetc() when stream is at offset 0) */
  c_m = mio_getpos (mio_m, &pos_m);
  assert_errno (errno, ==, EIO);
  c_f = mio_getpos (mio_f, &pos_f);
  assert_errno (errno, ==, EIO);
  g_assert_cmpint (c_m, ==, c_f);
  
  /* seek forward not to reproduce previous error */
  TEST_ACTION_2 (c, mio_seek, mio, 1, SEEK_SET, 0)
  
  loop (i, 3) {
    c_m = mio_getpos (mio_m, &pos_m);
    assert_errno (errno, ==, 0);
    c_f = mio_getpos (mio_f, &pos_f);
    assert_errno (errno, ==, 0);
    g_assert_cmpint (c_m, ==, c_f);
    TEST_ACTION_0 (c, mio_getc, mio, 0)
    g_assert_cmpint (c_m, ==, c_f);
    c_m = mio_setpos (mio_m, &pos_m);
    assert_errno (errno, ==, 0);
    c_f = mio_setpos (mio_f, &pos_f);
    assert_errno (errno, ==, 0);
    g_assert_cmpint (c_m, ==, c_f);
  }
  
  TEST_DESTROY_MIO (mio)
}



#define ADD_TEST_FUNC(section, name) \
  g_test_add_func ("/"#section"/"#name, test_##section##_##name)

int
main (int     argc,
      char  **argv)
{
  g_test_init (&argc, &argv, NULL);
  
  ADD_TEST_FUNC (read, read);
  ADD_TEST_FUNC (read, getc);
  ADD_TEST_FUNC (read, gets);
  //~ ADD_TEST_FUNC (write, write);
  //~ ADD_TEST_FUNC (write, putc);
  //~ ADD_TEST_FUNC (write, puts);
  ADD_TEST_FUNC (pos, tell);
  ADD_TEST_FUNC (pos, seek);
  ADD_TEST_FUNC (pos, rewind);
  ADD_TEST_FUNC (pos, getpos);
  ADD_TEST_FUNC (pos, setpos);
  
  g_test_run ();
  
  return 0;
}
