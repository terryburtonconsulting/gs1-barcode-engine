/**
 * GS1 barcode encoder application
 *
 * @author Copyright (c) 2000-2020 GS1 AISBL.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef DEBUG_H
#define DEBUG_H

#include "mtx.h"


#if PRNT


#define DEBUG_PRINT(...) do {				\
	printf(__VA_ARGS__);				\
} while (0)


#define DEBUG_PRINT_CWS(p,c,l) do {			\
	do_debug_print_cws(p,c,l);			\
} while (0)


#define DEBUG_PRINT_BITS(p,b,l) do {			\
	do_debug_print_bits(p,b,l);			\
} while (0)


#define DEBUG_PRINT_MATRIX(p,m,c,r) do {		\
	do_debug_print_matrix(p,m,c,r);			\
} while (0)


#define DEBUG_PRINT_PATTERN_LENGTHS(m,p,r) do {		\
	do_debug_print_pattern_lengths(m,p,r);		\
} while (0)


void do_debug_print_cws(char *prefix, uint8_t *cws, uint16_t cwslen);
void do_debug_print_bits(char* prefix, uint8_t *bits, int numbits);
void do_debug_print_matrix(char* prefix, uint8_t *mtx, int c, int r);
void do_debug_print_pattern_lengths(char *prefix, struct patternLength *pats, int rows);


#else

#define DEBUG_PRINT(...)
#define DEBUG_PRINT_CWS(p,c,l)
#define DEBUG_PRINT_MATRIX(m,c,r)
#define DEBUG_PRINT_PATTERN_LENGTHS(m,p,r)

#endif  /* PRNT */

#endif  /* DEBUG_H */
