
#ifndef H_FIO_H
#define H_FIO_H

#include <glib.h>
#include <stdio.h>


enum FIOType {
  FIO_TYPE_FILE,
  FIO_TYPE_MEMORY
};

typedef enum FIOType    FIOType;
typedef struct FIO      FIO;
typedef struct FIOPos   FIOPos;
/* should be GReallocFunc but it's only defined by GIO */
typedef gpointer (* FIOReallocFunc) (gpointer data,
                                     gsize    size);

struct FIOPos {
  guint type;
#ifdef FIO_DEBUG
  void *tag;
#endif
  union {
    fpos_t file;
    size_t mem;
  } impl;
};

#if 0
/* test for Geany's tag list */
struct FIO {
  int hack;
};
#endif

struct FIO {
  guint type;
  union {
    struct {
      FILE       *fp;
      gboolean    close;
    } file;
    struct {
      guchar         *buf;
      gsize           pos;
      gsize           size;
      gsize           allocated_size;
      FIOReallocFunc  realloc_func;
    } mem;
  } impl;
};


FIO        *fio_new_file    (const gchar *path,
                             const gchar *mode);
FIO        *fio_new_fp      (FILE        *fp,
                             gboolean     do_close);
FIO        *fio_new_memory  (guchar        *data,
                             gsize          size,
                             FIOReallocFunc realloc_func);
void        fio_free        (FIO *fio);
gsize       fio_read        (FIO     *fio,
                             void    *ptr,
                             gsize    size,
                             gsize    nmemb);
gsize       fio_write       (FIO         *fio,
                             const void  *ptr,
                             gsize        size,
                             gsize        nmemb);
gint        fio_getc        (FIO *fio);
char       *fio_gets        (FIO   *fio,
                             gchar *s,
                             gsize  size);
gint        fio_ungetc      (FIO *fio,
                             gint c);
gint        fio_putc        (FIO *fio,
                             gint c);
gint        fio_puts        (FIO         *fio,
                             const gchar *s);

gint        fio_clearerr    (FIO *fio);
gint        fio_eof         (FIO *fio);
gint        fio_error       (FIO *fio);
gint        fio_seek        (FIO   *fio,
                             glong  offset,
                             gint   whence);
glong       fio_tell        (FIO *fio);
void        fio_rewind      (FIO *fio);
gint        fio_getpos      (FIO     *fio,
                             FIOPos  *pos);
gint        fio_setpos      (FIO     *fio,
                             FIOPos  *pos);



#endif /* guard */