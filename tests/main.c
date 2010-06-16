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

#include <glib.h>
#include "fio/fio.h"

#define TEST_FILE "test.input"


static FIO *
test_fio_mem_new_from_file (const gchar *file,
                            gboolean     rw)
{
  FIO    *fio = NULL;
  gchar  *contents;
  gsize   length;
  GError *err = NULL;
  
  if (! g_file_get_contents (file, &contents, &length, &err)) {
    g_critical ("Failed to load file: %s", err->message);
    g_error_free (err);
  } else {
    fio = fio_new_memory ((guchar *)contents, length, rw ? g_try_realloc : NULL);
  }
  
  return fio;
}


#define range(var, start, end, step) var = start; var < end; var += step
#define range_loop(var, start, end, step) for (range(var, start, end, step))
#define loop(var, n) range_loop(var, 0, n, 1)


#define TEST_ACTION_0(ret_var, func, fio_var) \
  ret_var##_m = func (fio_var##_m);           \
  ret_var##_f = func (fio_var##_f);
#define TEST_ACTION_1(ret_var, func, fio_var, p1) \
  ret_var##_m = func (fio_var##_m, p1);           \
  ret_var##_f = func (fio_var##_f, p1);
#define TEST_ACTION_2(ret_var, func, fio_var, p1, p2) \
  ret_var##_m = func (fio_var##_m, p1, p2);           \
  ret_var##_f = func (fio_var##_f, p1, p2);

static void
test_read_getc (void)
{
  FIO *fio_m, *fio_f;
  gint c_m, c_f;
  gint i;
  
  fio_m = test_fio_mem_new_from_file (TEST_FILE, FALSE);
  fio_f = fio_new_file (TEST_FILE, "r");
  g_assert (fio_m != NULL && fio_f != NULL);
  
  loop (i, 3) {
    TEST_ACTION_0 (c, fio_getc, fio)
    g_assert (c_m == c_f);
  }
  TEST_ACTION_1 (c, fio_ungetc, fio, 'X')
  loop (i, 3) {
    TEST_ACTION_0 (c, fio_getc, fio)
    g_assert (c_m == c_f);
  }
}

static void
test_read_gets (void)
{
  FIO *fio_m, *fio_f;
  gchar s_m[255], s_f[255];
  gint c_m, c_f;
  gint i;
  
  fio_m = test_fio_mem_new_from_file (TEST_FILE, FALSE);
  fio_f = fio_new_file (TEST_FILE, "r");
  g_assert (fio_m != NULL && fio_f != NULL);
  
  loop (i, 3) {
    fio_gets (fio_m, s_m, 255);
    fio_gets (fio_f, s_f, 255);
    g_assert_cmpstr (s_m, ==, s_f);
  }
  TEST_ACTION_1 (c, fio_ungetc, fio, 'X')
  loop (i, 3) {
    fio_gets (fio_m, s_m, 255);
    fio_gets (fio_f, s_f, 255);
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
