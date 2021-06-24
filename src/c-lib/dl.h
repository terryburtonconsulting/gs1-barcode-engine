/**
 * GS1 barcode encoder library
 *
 * @author Copyright (c) 2021 GS1 AISBL.
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

#ifndef DL_H
#define DL_H

#include <stdbool.h>

#include "gs1encoders.h"

bool gs1_parseDLuri(gs1_encoder *ctx, char *dlData, char *dataStr);


#ifdef UNIT_TESTS

void test_dl_parseDLuri(void);
void test_dl_URIunescape(void);

#endif


#endif  /* DL_H */
