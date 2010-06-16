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
    mio = mio_new_memory ((guchar *)contents, length, rw ? g_try_realloc : NULL);
  }
  
  return mio;
}


#define range(var, start, end, step) var = start; var < end; var += step
#define range_loop(var, start, end, step) for (range(var, start, end, step))
#define loop(var, n) range_loop(var, 0, n, 1)


#define TEST_ACTION_0(ret_var, func, mio_var) \
  ret_var##_m = func (mio_var##_m);           \
  ret_var##_f = func (mio_var##_f);
#define TEST_ACTION_1(ret_var, func, mio_var, p1) \
  ret_var##_m = func (mio_var##_m, p1);           \
  ret_var##_f = func (mio_var##_f, p1);
#define TEST_ACTION_2(ret_var, func, mio_var, p1, p2) \
  ret_var##_m = func (mio_var##_m, p1, p2);           \
  ret_var##_f = func (mio_var##_f, p1, p2);

static void
test_read_getc (void)
{
  MIO *mio_m, *mio_f;
  gint c_m, c_f;
  gint i;
  
  mio_m = test_mio_mem_new_from_file (TEST_FILE, FALSE);
  mio_f = mio_new_file (TEST_FILE, "r");
  g_assert (mio_m != NULL && mio_f != NULL);
  
  loop (i, 3) {
    TEST_ACTION_0 (c, mio_getc, mio)
    g_assert (c_m == c_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X')
  loop (i, 3) {
    TEST_ACTION_0 (c, mio_getc, mio)
    g_assert (c_m == c_f);
  }
}

static void
test_read_gets (void)
{
  MIO *mio_m, *mio_f;
  gchar s_m[255], s_f[255];
  gint c_m, c_f;
  gint i;
  
  mio_m = test_mio_mem_new_from_file (TEST_FILE, FALSE);
  mio_f = mio_new_file (TEST_FILE, "r");
  g_assert (mio_m != NULL && mio_f != NULL);
  
  loop (i, 3) {
    mio_gets (mio_m, s_m, 255);
    mio_gets (mio_f, s_f, 255);
    g_assert_cmpstr (s_m, ==, s_f);
  }
  TEST_ACTION_1 (c, mio_ungetc, mio, 'X')
  loop (i, 3) {
    mio_gets (mio_m, s_m, 255);
    mio_gets (mio_f, s_f, 255);
    g_assert_cmpstr (s_m, ==, s_f);
  }
}


#define ADD_TEST_FUNC(section, name) \
  g_test_add_func ("/"#section"/"#name, test_##section##_##name)

int
main (int     argc,
      char  **argv)
{
  g_test_init (&argc, &argv, NULL);
  
  //~ ADD_TEST_FUNC (read, read);
  ADD_TEST_FUNC (read, getc);
  ADD_TEST_FUNC (read, gets);
  //~ ADD_TEST_FUNC (write, write);
  //~ ADD_TEST_FUNC (write, putc);
  //~ ADD_TEST_FUNC (write, puts);
  
  g_test_run ();
  
  return 0;
}
