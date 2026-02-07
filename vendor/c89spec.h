
/*********************************************************************/
/* 2015 Nikki Koole has copied , merged, modified and published this */
/*********************************************************************/

/*
Copyright (c) 2013 Ithai Levi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef C89SPEC_H_SC604JRD
#define C89SPEC_H_SC604JRD

/* profiler threshold                                                         */
#ifndef C89SPEC_PROFILE_THRESHOLD
#define C89SPEC_PROFILE_THRESHOLD 1.00                          /* in seconds */
#endif

/* MACRO and function definitions of c89spec:                                 */
/* -----------------------------                                              */
/* describe({module_name})                                                    */
/* it({module responsibility})                                                */
/* xit({module responsibility})                                               */
/* expect({scalar})                                                           */
/* test({module_name})                                                        */
/* summary()                                                                  */

/* "describe" encapsulates a set of "it" clauses in a function                */
/* MODULE should be a valid C function literal                                */
#define describe(MODULE) \
   void MODULE(void)

/* "expect" tests the passed scalar, and prints the result and time.          */
/* You can have several expect tests declared in a single "it" clause but     */
/* only one expect can be executed during the test.                           */
#define expect(SCALAR) \
   _c89spec_end_it(); \
   (SCALAR) ? _c89spec_expect_passed() \
            : _c89spec_expect_failed(#SCALAR);


/* "it" encapsulates a single test in a curly block.                          */
/* REQUIREMENT can be anything that's valid as string.                        */
#define it(REQUIREMENT) \
   _c89spec_begin_it(#REQUIREMENT);

/* "xit" marks a test as disabled (skipped) without executing expectations.   */
#define xit(REQUIREMENT) \
   _c89spec_skip(#REQUIREMENT); if(0)

/* "test" invokes a "describe" clause.                                        */
#define test(MODULE) \
   _c89spec_test_module(#MODULE,MODULE);

/* Use "summary" to optionally print the final tests counters and             */
/* return the tests final result                                              */
int summary(void);

/* Enable quiet mode - only show failures                                     */
void set_quiet_mode(int enabled);

/* private functions                                                          */
void _c89spec_test_module(const char * module,void (*func)(void));
void _c89spec_begin_it(const char * requirement);
void _c89spec_end_it(void);
void _c89spec_expect_passed(void);
void _c89spec_expect_failed(const char * scalar);
void _c89spec_skip(const char * requirement);

#endif /* end of include guard: C89SPEC_H_SC604JRD */

/*
Copyright (c) 2013 Ithai Levi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <time.h>


/* summary metrics                                                            */
static int _c89spec_tests_execs   = 0;
static int _c89spec_tests_passed  = 0;
static int _c89spec_tests_failed  = 0;
static int _c89spec_tests_skipped = 0;

/* quiet mode - only show failures                                            */
static int _c89spec_quiet_mode = 0;

/* spinner for quiet mode                                                     */
static const char _c89spec_spinner[] = "|/-\\";
static int _c89spec_spinner_index = 0;

/* store current requirement for potential failure reporting                   */
static const char * _c89spec_current_requirement = NULL;
static const char * _c89spec_current_module = NULL;

/* profiler global vars                                                       */
static clock_t _c89spec_clock_begin;
static clock_t _c89spec_clock_end;
static double  _c89spec_test_time;

/* formatting constants                                                       */

//#define C89SPEC_NO_FANCY_STUFF
#ifdef C89SPEC_NO_FANCY_STUFF
static const char * _C89SPEC_NO_COLOR    = "";
static const char * _C89SPEC_UNDERSCORE  = "";
static const char * _C89SPEC_RED_COLOR   = "";
static const char * _C89SPEC_GREEN_COLOR = "";
static const char * _C89SPEC_BLUE_COLOR  = "";
static const char * _C89SPEC_BLACK_COLOR = "";
#else
static const char * _C89SPEC_NO_COLOR    = "\033[0m";
static const char * _C89SPEC_UNDERSCORE  = "\033[4m";
static const char * _C89SPEC_RED_COLOR   = "\033[1;31m";
static const char * _C89SPEC_GREEN_COLOR = "\033[1;32m";
static const char * _C89SPEC_BLUE_COLOR  = "\033[1;34m";
static const char * _C89SPEC_BLACK_COLOR  = "\033[1;30m";
#endif

void _c89spec_test_module(const char * module,void (*func)(void)) {
   _c89spec_current_module = module;
   if (!_c89spec_quiet_mode) {
      printf("\n%s%s%s%s\n",_C89SPEC_UNDERSCORE \
                           ,_C89SPEC_BLUE_COLOR \
                           ,module \
                           ,_C89SPEC_NO_COLOR);
   }
   func();
   if (!_c89spec_quiet_mode) {
      printf("%s\n\n",_C89SPEC_NO_COLOR);
   }
}

void _c89spec_begin_it(const char * requirement) {
   _c89spec_tests_execs++;
   _c89spec_current_requirement = requirement;
   if (!_c89spec_quiet_mode) {
      printf("\n%s\t[?] %s", _C89SPEC_NO_COLOR, requirement);
   }
   _c89spec_clock_begin = clock();
}

void _c89spec_end_it(void) {
   _c89spec_clock_end = clock();
   _c89spec_test_time = (double)(_c89spec_clock_end - _c89spec_clock_begin)
                        / CLOCKS_PER_SEC;
   if (!_c89spec_quiet_mode) {
      (_c89spec_test_time > C89SPEC_PROFILE_THRESHOLD)
         ? printf("%s",_C89SPEC_RED_COLOR)
         : printf("%s",_C89SPEC_BLACK_COLOR);
      //printf(" (%.2f seconds)", _c89spec_test_time);
   }
}

void _c89spec_expect_passed(void) {
   if (!_c89spec_quiet_mode) {
      printf("\r\t%s[x]\t",_C89SPEC_GREEN_COLOR);
   }
   /* In quiet mode, show nothing */
   _c89spec_tests_passed++;
}

void _c89spec_expect_failed(const char * scalar) {
   if (_c89spec_quiet_mode) {
      /* In quiet mode, show F and then the failure with context */
      printf("%sF%s\n", _C89SPEC_RED_COLOR, _C89SPEC_NO_COLOR);
      if (_c89spec_current_module) {
         printf("\n%s%s%s%s\n", _C89SPEC_UNDERSCORE, _C89SPEC_BLUE_COLOR, 
                _c89spec_current_module, _C89SPEC_NO_COLOR);
      }
      printf("\t%s[ ] %s\n", _C89SPEC_RED_COLOR, _c89spec_current_requirement);
      printf("\t\t%s%s\n", _C89SPEC_RED_COLOR, scalar);
   } else {
      printf("\r\t%s[ ]\n\t\t%s", _C89SPEC_RED_COLOR, scalar);
   }
   _c89spec_tests_failed++;
}

void _c89spec_skip(const char * requirement) {
   _c89spec_tests_skipped++;
   if (!_c89spec_quiet_mode) {
      printf("\n%s\t[%s%s%s] %s", _C89SPEC_NO_COLOR,
             _C89SPEC_BLUE_COLOR, "s", _C89SPEC_NO_COLOR, requirement);
   }
}

void set_quiet_mode(int enabled) {
   _c89spec_quiet_mode = enabled;
}

int summary(void) {
   if (_c89spec_quiet_mode) {
      printf("\n");  // Add newline after dots
   }
   printf ("Total: %s%d%s\n",_C89SPEC_BLUE_COLOR
                            ,_c89spec_tests_execs + _c89spec_tests_skipped
                            ,_C89SPEC_NO_COLOR);

   printf ("\tPassed: %s%d%s\n",_C89SPEC_GREEN_COLOR
                               ,_c89spec_tests_passed
                               ,_C89SPEC_NO_COLOR);

   printf ("\tFailed: %s%d%s\n",_c89spec_tests_failed
                                  ? _C89SPEC_RED_COLOR
                                  : _C89SPEC_GREEN_COLOR
                               ,_c89spec_tests_failed
                               ,_C89SPEC_NO_COLOR);
   if (_c89spec_tests_skipped > 0) {
      printf ("\tDisabled: %s%d%s\n",_C89SPEC_BLUE_COLOR
                                    ,_c89spec_tests_skipped
                                    ,_C89SPEC_NO_COLOR);
   }
   printf ("\n");
   return _c89spec_tests_failed;
}
