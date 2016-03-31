/*
 * Copyright 2016 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mongo-c-performance.h"

#include <bson.h>
#include <mongoc.h>


const int NUM_ITERATIONS        = 100;
const int NUM_DOCS            = 10000;

static int    g_num_tests;
static char **g_test_names;


void
parse_args (int    argc,
            char **argv)
{
   g_num_tests = argc - 1;
   g_test_names = g_num_tests ? &argv[1] : NULL;
}


void
read_json_file (const char *data_path,
                bson_t     *bson)
{
   char *path;
   bson_json_reader_t *reader;
   bson_error_t error;
   int r;

   path = bson_strdup_printf ("performance-testdata/%s", data_path);
   reader = bson_json_reader_new_from_file (path, &error);
   if (!reader) {
      MONGOC_ERROR ("%s: %s\n", path, error.message);
      abort ();
   }

   bson_init (bson);
   r = bson_json_reader_read (reader, bson, &error);
   if (r < 0) {
      MONGOC_ERROR ("%s: %s\n", data_path, error.message);
      abort ();
   }

   if (r == 0) {
      MONGOC_ERROR ("%s: no data\n", data_path);
      abort ();
   }

   bson_json_reader_destroy (reader);
   bson_free (path);
}


/* from "man qsort" */
static int
cmp (const void *a,
     const void *b)
{
   int64_t arg1 = *(const int64_t *) a;
   int64_t arg2 = *(const int64_t *) b;

   if (arg1 < arg2) return -1;
   if (arg1 > arg2) return 1;
   return 0;
}


static bool
should_run_test (const char *name)
{
   int i;

   if (!g_test_names) {
      /* run all tests */
      return true;
   }

   for (i = 0; i < g_num_tests; i++) {
      if (!strcmp (g_test_names[i], name)) {
         return true;
      }
   }

   return false;
}


void
run_perf_tests (perf_test_t *tests)
{
   perf_test_t *test;
   int64_t *results;
   int i;
   int64_t start;
   double median;

   test = tests;
   results = bson_malloc (NUM_ITERATIONS * sizeof (int64_t));

   while (test->name) {
      if (should_run_test (test->name)) {
         test->context = bson_malloc0 (test->context_sz);
         if (test->setup) {
            test->setup (test);
         }

         for (i = 0; i < NUM_ITERATIONS; i++) {
            if (test->before) {
               test->before (test);
            }

            start = bson_get_monotonic_time ();
            test->task (test);
            results[i] = bson_get_monotonic_time () - start;

            if (test->after) {
               test->after (test);
            }
         }

         if (test->teardown) {
            test->teardown (test);
         }

         qsort ((void *) results, NUM_ITERATIONS, sizeof (int64_t), cmp);
         median = (double) (results[NUM_ITERATIONS / 2 - 1]) / 1e3;
         printf ("%25s, %f\n", test->name, median);

         bson_free (test->context);
      }

      test++;
   }

   bson_free (results);
}
