/* Keep a unique copy of strings.

   Copyright (C) 2002-2005, 2009-2011 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include "system.h"

#include <error.h>
#include <hash.h>
#include <quotearg.h>
#include <stdarg.h>

#include "uniqstr.h"

/*-----------------------.
| A uniqstr hash table.  |
`-----------------------*/

/* Initial capacity of uniqstr hash table.  */
#define HT_INITIAL_CAPACITY 257

static struct hash_table *uniqstrs_table = NULL;

/*-------------------------------------.
| Create the uniqstr for S if needed.  |
`-------------------------------------*/

uniqstr
uniqstr_new (char const *str)
{
  uniqstr res = hash_lookup (uniqstrs_table, str);
  if (!res)
    {
      /* First insertion in the hash. */
      res = xstrdup (str);
      if (!hash_insert (uniqstrs_table, res))
        xalloc_die ();
    }
  return res;
}

/* arg list should be with char* only and end with NULL */
char*
uniqstr_get_format (char const *aaa, ...)
{
  static char format[50] = {0};
  char* arg = NULL;
  int i = 0;
  va_list args;

  va_start (args, aaa);
  arg = va_arg(args, char*);
  while(arg) {
	  format[i++] = '%';
	  format[i++] = 's';
	  arg = va_arg(args, char*);
  }
  va_end (args);

  format[i] = 0;

  return format;
}

uniqstr
uniqstr_vsprintf (char const *format, ...)
{
  char *res = NULL;
  va_list args;
  size_t length;
  uniqstr result;

  va_start (args, format);
  length = vsnprintf (NULL, 0, format, args);
  va_end (args);

  res = malloc(sizeof(char)*(length+1));
  //char res[length + 1];
  va_start (args, format);
  vsprintf (res, format, args);
  va_end (args);

  result = uniqstr_new (res);

  free(res);

  return result;//uniqstr_new (res);
}

/*------------------------------.
| Abort if S is not a uniqstr.  |
`------------------------------*/

void
uniqstr_assert (char const *str)
{
  if (!hash_lookup (uniqstrs_table, str))
    {
      error (0, 0,
	     "not a uniqstr: %s", quotearg (str));
      abort ();
    }
}


/*--------------------.
| Print the uniqstr.  |
`--------------------*/

static  bool
uniqstr_print (uniqstr ustr)
{
  fprintf (stderr, "%s\n", ustr);
  return true;
}

static bool
uniqstr_print_processor (void *ustr, void *null ATTRIBUTE_UNUSED)
{
  return uniqstr_print (ustr);
}


/*-----------------------.
| A uniqstr hash table.  |
`-----------------------*/

static bool
hash_compare_uniqstr (void const *m1, void const *m2)
{
  return strcmp (m1, m2) == 0;
}

static size_t
hash_uniqstr (void const *m, size_t tablesize)
{
  return hash_string (m, tablesize);
}

/*----------------------------.
| Create the uniqstrs table.  |
`----------------------------*/

void
uniqstrs_new (void)
{
  uniqstrs_table = hash_initialize (HT_INITIAL_CAPACITY,
				    NULL,
				    hash_uniqstr,
				    hash_compare_uniqstr,
				    free);
}


/*-------------------------------------.
| Perform a task on all the uniqstrs.  |
`-------------------------------------*/

static void
uniqstrs_do (Hash_processor processor, void *processor_data)
{
  hash_do_for_each (uniqstrs_table, processor, processor_data);
}


/*-----------------.
| Print them all.  |
`-----------------*/

void
uniqstrs_print (void)
{
  uniqstrs_do (uniqstr_print_processor, NULL);
}


/*--------------------.
| Free the uniqstrs.  |
`--------------------*/

void
uniqstrs_free (void)
{
  hash_free (uniqstrs_table);
}
