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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gs1encoders.h"
#include "enc-private.h"
#include "debug.h"
#include "gs1.h"


/*
 *  This library stores a compact representation of AI data (FNC1 in first) in
 *  unbracketed format where "#" represents FNC1, i.e. "#..."
 *
 *  Where applicable, ingested data is validated and processed into the
 *  aforementioned form, then a table of extracted AIs is maintained consisting
 *  of a pointer to an AI table entry, a pointer to the data start of the AI
 *  value, and the value's length:
 *
 *    aiEntry -> aiTable entry
 *    value   -> Start of value in the AI data string
 *    vallen  : Length of value
 *
 *  This ensures that we only store a single instance of the input that has
 *  been provided by the provided by the user, whether they have provided
 *  bracketed AI data or scan data.
 *
 *  Digital Link inputs are an exception since they must be stored as given,
 *  ready to be encoded directly into a barcode symbol. Unlike conversion
 *  between bracketed/unbracketed AI data and scan data, by specification the
 *  conversion between Digital Link URIs and AI syntax is not bijective: the
 *  URI stem is lost, element order may not be preserved and AI values may be
 *  normalised into canonical form.

 *  The extracted AI element string is stored in dlAIbuffer which is used as
 *  the storage for HRI text and construction bracketed AI strings.
 *
 */


#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof(x[0]))


/*
 *  AI prefixes that are defined as not requiring termination by an FNC1 character
 *
 */
static const char *fixedAIprefixes[22] = {
	"00", "01", "02",
	"03", "04",
	"11", "12", "13", "14", "15", "16", "17", "18", "19",
	"20",
	// "23",	// No longer defined as fixed length
	"31", "32", "33", "34", "35", "36",
	"41"
};


/*
 *  Set of characters that are permissible in URIs, including percent
 *
 */
static const char *uriCharacters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%";


/*
 *  Set of 82 characters valid within type "X" AIs
 *
 */
static const char *cset82 = "!\"%&'()*+,-./0123456789:;<=>?ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";


/* "Linter" functions
 *
 * Used to validate AI components
 *
 */

static bool lint_cset82(gs1_encoder *ctx, const struct aiEntry *entry, char *val) {
	DEBUG_PRINT("      cset82...");
	if (strspn(val, cset82) != strlen(val)) {
		sprintf(ctx->errMsg, "AI (%s): Incorrect CSET 82 character", entry->ai);
		ctx->errFlag = true;
		return false;
	}
	DEBUG_PRINT(" success\n");
	return true;
}


static bool lint_csetNumeric(gs1_encoder *ctx, const struct aiEntry *entry, char *val) {
	DEBUG_PRINT("      csetNumeric...");
	if (!gs1_allDigits((uint8_t*)val, 0)) {
		sprintf(ctx->errMsg, "AI (%s): Illegal non-digit character", entry->ai);
		ctx->errFlag = true;
		return false;
	}
	DEBUG_PRINT(" success\n");
	return true;
}


static bool lint_csum(gs1_encoder *ctx, const struct aiEntry *entry, char *val) {
	DEBUG_PRINT("      csum...");
	if (!gs1_validateParity((uint8_t*)val)) {
		DEBUG_PRINT(" failed\n");
		sprintf(ctx->errMsg, "AI (%s): Incorrect check digit", entry->ai);
		ctx->errFlag = true;
		return false;
	};
	DEBUG_PRINT(" success\n");
	return true;
}


#define FNC1 true
#define NO_FNC1 false


#define AI_VA(a, f, c1,mn1,mx1,l01, c2,mn2,mx2,l02, c3,mn3,mx3,l03, c4,mn4,mx4,l04, c5,mn5,mx5,l05, t) {	\
		.ai = a,											\
		.fnc1 = f,											\
		.parts = {											\
			{ .cset = cset_##c1, .min = mn1, .max = mx1, .linters[0] = lint_##l01 },		\
			{ .cset = cset_##c2, .min = mn2, .max = mx2, .linters[0] = lint_##l02 },		\
			{ .cset = cset_##c3, .min = mn3, .max = mx3, .linters[0] = lint_##l03 },		\
			{ .cset = cset_##c4, .min = mn4, .max = mx4, .linters[0] = lint_##l04 },		\
			{ .cset = cset_##c5, .min = mn5, .max = mx5, .linters[0] = lint_##l05 },		\
		},												\
		.title = t,											\
	}
#define PASS_ON(...) __VA_ARGS__
#define AI(...) PASS_ON(AI_VA(__VA_ARGS__))
#define cset_0 0
#define lint__ 0
#define __ 0,0,0,_

static const struct aiEntry ai_table[] = {
	AI( "00"  , NO_FNC1, N,18,18,csum, __, __, __, __,                "SSCC"                      ),
	AI( "01"  , NO_FNC1, N,14,14,csum, __, __, __, __,                "GTIN"                      ),
	AI( "02"  , NO_FNC1, N,14,14,csum, __, __, __, __,                "CONTENT"                   ),
	AI( "10"  , FNC1   , X,1,20,_, __, __, __, __,                    "BATCH/LOT"                 ),
	AI( "11"  , NO_FNC1, N,6,6,_, __, __, __, __,                     "PROD DATE"                 ),
	AI( "12"  , NO_FNC1, N,6,6,_, __, __, __, __,                     "DUE DATE"                  ),
	AI( "13"  , NO_FNC1, N,6,6,_, __, __, __, __,                     "PACK DATE"                 ),
	AI( "15"  , NO_FNC1, N,6,6,_, __, __, __, __,                     "BEST BEFORE or BEST BY"    ),
	AI( "16"  , NO_FNC1, N,6,6,_, __, __, __, __,                     "SELL BY"                   ),
	AI( "17"  , NO_FNC1, N,6,6,_, __, __, __, __,                     "USE BY or EXPIRY"          ),
	AI( "20"  , NO_FNC1, N,2,2,_, __, __, __, __,                     "VARIANT"                   ),
	AI( "21"  , FNC1   , X,1,20,_, __, __, __, __,                    "SERIAL"                    ),
	AI( "22"  , FNC1   , X,1,20,_, __, __, __, __,                    "CPV"                       ),
	AI( "235" , FNC1   , X,1,28,_, __, __, __, __,                    "TPX"                       ),
	AI( "240" , FNC1   , X,1,30,_, __, __, __, __,                    "ADDITIONAL ID"             ),
	AI( "241" , FNC1   , X,1,30,_, __, __, __, __,                    "CUST. PART NO."            ),
	AI( "242" , FNC1   , N,1,6,_, __, __, __, __,                     "MTO VARIANT"               ),
	AI( "243" , FNC1   , X,1,20,_, __, __, __, __,                    "PCN"                       ),
	AI( "250" , FNC1   , X,1,30,_, __, __, __, __,                    "SECONDARY SERIAL"          ),
	AI( "251" , FNC1   , X,1,30,_, __, __, __, __,                    "REF. TO SOURCE"            ),
	AI( "253" , FNC1   , N,13,13,csum, X,0,17,_, __, __, __,          "GDTI"                      ),
	AI( "254" , FNC1   , X,1,20,_, __, __, __, __,                    "GLN EXTENSION COMPONENT"   ),
	AI( "255" , FNC1   , N,13,13,csum, N,0,12,_, __, __, __,          "GCN"                       ),
	AI( "30"  , FNC1   , N,1,8,_, __, __, __, __,                     "VAR. COUNT"                ),
	AI( "3100", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (kg)"           ),
	AI( "3101", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (kg)"           ),
	AI( "3102", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (kg)"           ),
	AI( "3103", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (kg)"           ),
	AI( "3104", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (kg)"           ),
	AI( "3105", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (kg)"           ),
	AI( "3110", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m)"                ),
	AI( "3111", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m)"                ),
	AI( "3112", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m)"                ),
	AI( "3113", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m)"                ),
	AI( "3114", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m)"                ),
	AI( "3115", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m)"                ),
	AI( "3120", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m)"                 ),
	AI( "3121", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m)"                 ),
	AI( "3122", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m)"                 ),
	AI( "3123", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m)"                 ),
	AI( "3124", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m)"                 ),
	AI( "3125", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m)"                 ),
	AI( "3130", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m)"                ),
	AI( "3131", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m)"                ),
	AI( "3132", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m)"                ),
	AI( "3133", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m)"                ),
	AI( "3134", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m)"                ),
	AI( "3135", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m)"                ),
	AI( "3140", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2)"                ),
	AI( "3141", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2)"                ),
	AI( "3142", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2)"                ),
	AI( "3143", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2)"                ),
	AI( "3144", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2)"                ),
	AI( "3145", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2)"                ),
	AI( "3150", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (l)"            ),
	AI( "3151", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (l)"            ),
	AI( "3152", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (l)"            ),
	AI( "3153", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (l)"            ),
	AI( "3154", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (l)"            ),
	AI( "3155", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (l)"            ),
	AI( "3160", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (m^3)"          ),
	AI( "3161", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (m^3)"          ),
	AI( "3162", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (m^3)"          ),
	AI( "3163", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (m^3)"          ),
	AI( "3164", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (m^3)"          ),
	AI( "3165", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (m^3)"          ),
	AI( "3200", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (lb)"           ),
	AI( "3201", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (lb)"           ),
	AI( "3202", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (lb)"           ),
	AI( "3203", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (lb)"           ),
	AI( "3204", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (lb)"           ),
	AI( "3205", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (lb)"           ),
	AI( "3210", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i)"                ),
	AI( "3211", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i)"                ),
	AI( "3212", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i)"                ),
	AI( "3213", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i)"                ),
	AI( "3214", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i)"                ),
	AI( "3215", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i)"                ),
	AI( "3220", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f)"                ),
	AI( "3221", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f)"                ),
	AI( "3222", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f)"                ),
	AI( "3223", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f)"                ),
	AI( "3224", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f)"                ),
	AI( "3225", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f)"                ),
	AI( "3230", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y)"                ),
	AI( "3231", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y)"                ),
	AI( "3232", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y)"                ),
	AI( "3233", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y)"                ),
	AI( "3234", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y)"                ),
	AI( "3235", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y)"                ),
	AI( "3240", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i)"                 ),
	AI( "3241", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i)"                 ),
	AI( "3242", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i)"                 ),
	AI( "3243", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i)"                 ),
	AI( "3244", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i)"                 ),
	AI( "3245", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i)"                 ),
	AI( "3250", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f)"                 ),
	AI( "3251", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f)"                 ),
	AI( "3252", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f)"                 ),
	AI( "3253", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f)"                 ),
	AI( "3254", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f)"                 ),
	AI( "3255", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f)"                 ),
	AI( "3260", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y)"                 ),
	AI( "3261", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y)"                 ),
	AI( "3262", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y)"                 ),
	AI( "3263", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y)"                 ),
	AI( "3264", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y)"                 ),
	AI( "3265", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y)"                 ),
	AI( "3270", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i)"                ),
	AI( "3271", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i)"                ),
	AI( "3272", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i)"                ),
	AI( "3273", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i)"                ),
	AI( "3274", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i)"                ),
	AI( "3275", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i)"                ),
	AI( "3280", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f)"                ),
	AI( "3281", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f)"                ),
	AI( "3282", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f)"                ),
	AI( "3283", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f)"                ),
	AI( "3284", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f)"                ),
	AI( "3285", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f)"                ),
	AI( "3290", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y)"                ),
	AI( "3291", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y)"                ),
	AI( "3292", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y)"                ),
	AI( "3293", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y)"                ),
	AI( "3294", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y)"                ),
	AI( "3295", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y)"                ),
	AI( "3300", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (kg)"         ),
	AI( "3301", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (kg)"         ),
	AI( "3302", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (kg)"         ),
	AI( "3303", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (kg)"         ),
	AI( "3304", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (kg)"         ),
	AI( "3305", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (kg)"         ),
	AI( "3310", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m), log"           ),
	AI( "3311", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m), log"           ),
	AI( "3312", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m), log"           ),
	AI( "3313", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m), log"           ),
	AI( "3314", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m), log"           ),
	AI( "3315", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (m), log"           ),
	AI( "3320", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m), log"            ),
	AI( "3321", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m), log"            ),
	AI( "3322", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m), log"            ),
	AI( "3323", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m), log"            ),
	AI( "3324", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m), log"            ),
	AI( "3325", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (m), log"            ),
	AI( "3330", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m), log"           ),
	AI( "3331", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m), log"           ),
	AI( "3332", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m), log"           ),
	AI( "3333", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m), log"           ),
	AI( "3334", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m), log"           ),
	AI( "3335", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (m), log"           ),
	AI( "3340", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2), log"           ),
	AI( "3341", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2), log"           ),
	AI( "3342", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2), log"           ),
	AI( "3343", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2), log"           ),
	AI( "3344", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2), log"           ),
	AI( "3345", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (m^2), log"           ),
	AI( "3350", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (l), log"           ),
	AI( "3351", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (l), log"           ),
	AI( "3352", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (l), log"           ),
	AI( "3353", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (l), log"           ),
	AI( "3354", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (l), log"           ),
	AI( "3355", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (l), log"           ),
	AI( "3360", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (m^3), log"         ),
	AI( "3361", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (m^3), log"         ),
	AI( "3362", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (m^3), log"         ),
	AI( "3363", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (m^3), log"         ),
	AI( "3364", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (m^3), log"         ),
	AI( "3365", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (m^3), log"         ),
	AI( "3370", NO_FNC1, N,6,6,_, __, __, __, __,                     "KG PER m^2"                ),
	AI( "3371", NO_FNC1, N,6,6,_, __, __, __, __,                     "KG PER m^2"                ),
	AI( "3372", NO_FNC1, N,6,6,_, __, __, __, __,                     "KG PER m^2"                ),
	AI( "3373", NO_FNC1, N,6,6,_, __, __, __, __,                     "KG PER m^2"                ),
	AI( "3374", NO_FNC1, N,6,6,_, __, __, __, __,                     "KG PER m^2"                ),
	AI( "3375", NO_FNC1, N,6,6,_, __, __, __, __,                     "KG PER m^2"                ),
	AI( "3400", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (lb)"         ),
	AI( "3401", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (lb)"         ),
	AI( "3402", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (lb)"         ),
	AI( "3403", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (lb)"         ),
	AI( "3404", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (lb)"         ),
	AI( "3405", NO_FNC1, N,6,6,_, __, __, __, __,                     "GROSS WEIGHT (lb)"         ),
	AI( "3410", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i), log"           ),
	AI( "3411", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i), log"           ),
	AI( "3412", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i), log"           ),
	AI( "3413", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i), log"           ),
	AI( "3414", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i), log"           ),
	AI( "3415", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (i), log"           ),
	AI( "3420", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f), log"           ),
	AI( "3421", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f), log"           ),
	AI( "3422", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f), log"           ),
	AI( "3423", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f), log"           ),
	AI( "3424", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f), log"           ),
	AI( "3425", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (f), log"           ),
	AI( "3430", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y), log"           ),
	AI( "3431", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y), log"           ),
	AI( "3432", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y), log"           ),
	AI( "3433", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y), log"           ),
	AI( "3434", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y), log"           ),
	AI( "3435", NO_FNC1, N,6,6,_, __, __, __, __,                     "LENGTH (y), log"           ),
	AI( "3440", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i), log"            ),
	AI( "3441", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i), log"            ),
	AI( "3442", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i), log"            ),
	AI( "3443", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i), log"            ),
	AI( "3444", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i), log"            ),
	AI( "3445", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (i), log"            ),
	AI( "3450", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f), log"            ),
	AI( "3451", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f), log"            ),
	AI( "3452", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f), log"            ),
	AI( "3453", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f), log"            ),
	AI( "3454", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f), log"            ),
	AI( "3455", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (f), log"            ),
	AI( "3460", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y), log"            ),
	AI( "3461", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y), log"            ),
	AI( "3462", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y), log"            ),
	AI( "3463", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y), log"            ),
	AI( "3464", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y), log"            ),
	AI( "3465", NO_FNC1, N,6,6,_, __, __, __, __,                     "WIDTH (y), log"            ),
	AI( "3470", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i), log"           ),
	AI( "3471", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i), log"           ),
	AI( "3472", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i), log"           ),
	AI( "3473", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i), log"           ),
	AI( "3474", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i), log"           ),
	AI( "3475", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (i), log"           ),
	AI( "3480", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f), log"           ),
	AI( "3481", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f), log"           ),
	AI( "3482", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f), log"           ),
	AI( "3483", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f), log"           ),
	AI( "3484", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f), log"           ),
	AI( "3485", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (f), log"           ),
	AI( "3490", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y), log"           ),
	AI( "3491", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y), log"           ),
	AI( "3492", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y), log"           ),
	AI( "3493", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y), log"           ),
	AI( "3494", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y), log"           ),
	AI( "3495", NO_FNC1, N,6,6,_, __, __, __, __,                     "HEIGHT (y), log"           ),
	AI( "3500", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2)"                ),
	AI( "3501", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2)"                ),
	AI( "3502", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2)"                ),
	AI( "3503", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2)"                ),
	AI( "3504", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2)"                ),
	AI( "3505", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2)"                ),
	AI( "3510", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2)"                ),
	AI( "3511", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2)"                ),
	AI( "3512", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2)"                ),
	AI( "3513", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2)"                ),
	AI( "3514", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2)"                ),
	AI( "3515", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2)"                ),
	AI( "3520", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2)"                ),
	AI( "3521", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2)"                ),
	AI( "3522", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2)"                ),
	AI( "3523", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2)"                ),
	AI( "3524", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2)"                ),
	AI( "3525", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2)"                ),
	AI( "3530", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2), log"           ),
	AI( "3531", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2), log"           ),
	AI( "3532", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2), log"           ),
	AI( "3533", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2), log"           ),
	AI( "3534", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2), log"           ),
	AI( "3535", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (i^2), log"           ),
	AI( "3540", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2), log"           ),
	AI( "3541", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2), log"           ),
	AI( "3542", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2), log"           ),
	AI( "3543", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2), log"           ),
	AI( "3544", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2), log"           ),
	AI( "3545", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (f^2), log"           ),
	AI( "3550", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2), log"           ),
	AI( "3551", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2), log"           ),
	AI( "3552", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2), log"           ),
	AI( "3553", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2), log"           ),
	AI( "3554", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2), log"           ),
	AI( "3555", NO_FNC1, N,6,6,_, __, __, __, __,                     "AREA (y^2), log"           ),
	AI( "3560", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (t)"            ),
	AI( "3561", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (t)"            ),
	AI( "3562", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (t)"            ),
	AI( "3563", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (t)"            ),
	AI( "3564", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (t)"            ),
	AI( "3565", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET WEIGHT (t)"            ),
	AI( "3570", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (oz)"           ),
	AI( "3571", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (oz)"           ),
	AI( "3572", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (oz)"           ),
	AI( "3573", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (oz)"           ),
	AI( "3574", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (oz)"           ),
	AI( "3575", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (oz)"           ),
	AI( "3600", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (q)"            ),
	AI( "3601", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (q)"            ),
	AI( "3602", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (q)"            ),
	AI( "3603", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (q)"            ),
	AI( "3604", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (q)"            ),
	AI( "3605", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (q)"            ),
	AI( "3610", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (g)"            ),
	AI( "3611", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (g)"            ),
	AI( "3612", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (g)"            ),
	AI( "3613", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (g)"            ),
	AI( "3614", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (g)"            ),
	AI( "3615", NO_FNC1, N,6,6,_, __, __, __, __,                     "NET VOLUME (g)"            ),
	AI( "3620", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (q), log"           ),
	AI( "3621", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (q), log"           ),
	AI( "3622", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (q), log"           ),
	AI( "3623", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (q), log"           ),
	AI( "3624", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (q), log"           ),
	AI( "3625", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (q), log"           ),
	AI( "3630", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (g), log"           ),
	AI( "3631", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (g), log"           ),
	AI( "3632", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (g), log"           ),
	AI( "3633", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (g), log"           ),
	AI( "3634", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (g), log"           ),
	AI( "3635", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (g), log"           ),
	AI( "3640", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3)"              ),
	AI( "3641", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3)"              ),
	AI( "3642", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3)"              ),
	AI( "3643", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3)"              ),
	AI( "3644", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3)"              ),
	AI( "3645", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3)"              ),
	AI( "3650", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3)"              ),
	AI( "3651", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3)"              ),
	AI( "3652", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3)"              ),
	AI( "3653", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3)"              ),
	AI( "3654", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3)"              ),
	AI( "3655", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3)"              ),
	AI( "3660", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3)"              ),
	AI( "3661", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3)"              ),
	AI( "3662", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3)"              ),
	AI( "3663", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3)"              ),
	AI( "3664", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3)"              ),
	AI( "3665", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3)"              ),
	AI( "3670", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3), log"         ),
	AI( "3671", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3), log"         ),
	AI( "3672", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3), log"         ),
	AI( "3673", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3), log"         ),
	AI( "3674", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3), log"         ),
	AI( "3675", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (i^3), log"         ),
	AI( "3680", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3), log"         ),
	AI( "3681", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3), log"         ),
	AI( "3682", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3), log"         ),
	AI( "3683", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3), log"         ),
	AI( "3684", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3), log"         ),
	AI( "3685", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (f^3), log"         ),
	AI( "3690", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3), log"         ),
	AI( "3691", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3), log"         ),
	AI( "3692", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3), log"         ),
	AI( "3693", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3), log"         ),
	AI( "3694", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3), log"         ),
	AI( "3695", NO_FNC1, N,6,6,_, __, __, __, __,                     "VOLUME (y^3), log"         ),
	AI( "37"  , FNC1   , N,1,8,_, __, __, __, __,                     "COUNT"                     ),
	AI( "3900", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3901", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3902", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3903", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3904", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3905", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3906", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3907", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3908", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3909", FNC1   , N,1,15,_, __, __, __, __,                    "AMOUNT"                    ),
	AI( "3910", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3911", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3912", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3913", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3914", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3915", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3916", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3917", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3918", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3919", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "AMOUNT"                    ),
	AI( "3920", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3921", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3922", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3923", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3924", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3925", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3926", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3927", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3928", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3929", FNC1   , N,1,15,_, __, __, __, __,                    "PRICE"                     ),
	AI( "3930", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3931", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3932", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3933", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3934", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3935", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3936", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3937", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3938", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3939", FNC1   , N,3,3,_, N,1,15,_, __, __, __,               "PRICE"                     ),
	AI( "3940", FNC1   , N,4,4,_, __, __, __, __,                     "PRCNT OFF"                 ),
	AI( "3941", FNC1   , N,4,4,_, __, __, __, __,                     "PRCNT OFF"                 ),
	AI( "3942", FNC1   , N,4,4,_, __, __, __, __,                     "PRCNT OFF"                 ),
	AI( "3943", FNC1   , N,4,4,_, __, __, __, __,                     "PRCNT OFF"                 ),
	AI( "3950", FNC1   , N,6,6,_, __, __, __, __,                     "PRICE/UoM"                 ),
	AI( "3951", FNC1   , N,6,6,_, __, __, __, __,                     "PRICE/UoM"                 ),
	AI( "3952", FNC1   , N,6,6,_, __, __, __, __,                     "PRICE/UoM"                 ),
	AI( "3953", FNC1   , N,6,6,_, __, __, __, __,                     "PRICE/UoM"                 ),
	AI( "3954", FNC1   , N,6,6,_, __, __, __, __,                     "PRICE/UoM"                 ),
	AI( "3955", FNC1   , N,6,6,_, __, __, __, __,                     "PRICE/UoM"                 ),
	AI( "400" , FNC1   , X,1,30,_, __, __, __, __,                    "ORDER NUMBER"              ),
	AI( "401" , FNC1   , X,1,30,_, __, __, __, __,                    "GINC"                      ),
	AI( "402" , FNC1   , N,17,17,csum, __, __, __, __,                "GSIN"                      ),
	AI( "403" , FNC1   , X,1,30,_, __, __, __, __,                    "ROUTE"                     ),
	AI( "410" , NO_FNC1, N,13,13,csum, __, __, __, __,                "SHIP TO LOC"               ),
	AI( "411" , NO_FNC1, N,13,13,csum, __, __, __, __,                "BILL TO"                   ),
	AI( "412" , NO_FNC1, N,13,13,csum, __, __, __, __,                "PURCHASE FROM"             ),
	AI( "413" , NO_FNC1, N,13,13,csum, __, __, __, __,                "SHIP FOR LOC"              ),
	AI( "414" , NO_FNC1, N,13,13,csum, __, __, __, __,                "LOC NO."                   ),
	AI( "415" , NO_FNC1, N,13,13,csum, __, __, __, __,                "PAY TO"                    ),
	AI( "416" , NO_FNC1, N,13,13,csum, __, __, __, __,                "PROD/SERV LOC"             ),
	AI( "417" , NO_FNC1, N,13,13,csum, __, __, __, __,                "PARTY"                     ),
	AI( "420" , FNC1   , X,1,20,_, __, __, __, __,                    "SHIP TO POST"              ),
	AI( "421" , FNC1   , N,3,3,_, X,1,9,_, __, __, __,                "SHIP TO POST"              ),
	AI( "422" , FNC1   , N,3,3,_, __, __, __, __,                     "ORIGIN"                    ),
	AI( "423" , FNC1   , N,3,15,_, __, __, __, __,                    "COUNTRY - INITIAL PROCESS" ),
	AI( "424" , FNC1   , N,3,3,_, __, __, __, __,                     "COUNTRY - PROCESS"         ),
	AI( "425" , FNC1   , N,3,15,_, __, __, __, __,                    "COUNTRY - DISASSEMBLY"     ),
	AI( "426" , FNC1   , N,3,3,_, __, __, __, __,                     "COUNTRY - FULL PROCESS"    ),
	AI( "427" , FNC1   , X,1,3,_, __, __, __, __,                     "ORIGIN SUBDIVISION"        ),
	AI( "4300", FNC1   , X,1,35,_, __, __, __, __,                    "SHIP TO COMP"              ),
	AI( "4301", FNC1   , X,1,35,_, __, __, __, __,                    "SHIP TO NAME"              ),
	AI( "4302", FNC1   , X,1,70,_, __, __, __, __,                    "SHIP TO ADD1"              ),
	AI( "4303", FNC1   , X,1,70,_, __, __, __, __,                    "SHIP TO ADD2"              ),
	AI( "4304", FNC1   , X,1,70,_, __, __, __, __,                    "SHIP TO SUB"               ),
	AI( "4305", FNC1   , X,1,70,_, __, __, __, __,                    "SHIP TO LOC"               ),
	AI( "4306", FNC1   , X,1,70,_, __, __, __, __,                    "SHIP TO REG"               ),
	AI( "4307", FNC1   , X,2,2,_, __, __, __, __,                     "SHIP TO COUNTRY"           ),
	AI( "4308", FNC1   , X,1,30,_, __, __, __, __,                    "SHIP TO PHONE"             ),
	AI( "4310", FNC1   , X,1,35,_, __, __, __, __,                    "RTN TO COMP"               ),
	AI( "4311", FNC1   , X,1,35,_, __, __, __, __,                    "RTN TO NAME"               ),
	AI( "4312", FNC1   , X,1,70,_, __, __, __, __,                    "RTN TO ADD1"               ),
	AI( "4313", FNC1   , X,1,70,_, __, __, __, __,                    "RTN TO ADD2"               ),
	AI( "4314", FNC1   , X,1,70,_, __, __, __, __,                    "RTN TO SUB"                ),
	AI( "4315", FNC1   , X,1,70,_, __, __, __, __,                    "RTN TO LOC"                ),
	AI( "4316", FNC1   , X,1,70,_, __, __, __, __,                    "RTN TO REG"                ),
	AI( "4317", FNC1   , X,2,2,_, __, __, __, __,                     "RTN TO COUNTRY"            ),
	AI( "4318", FNC1   , X,1,20,_, __, __, __, __,                    "RTN TO POST"               ),
	AI( "4319", FNC1   , X,1,30,_, __, __, __, __,                    "RTN TO PHONE"              ),
	AI( "4320", FNC1   , X,1,35,_, __, __, __, __,                    "SRV DESCRIPTION"           ),
	AI( "4321", FNC1   , N,1,1,_, __, __, __, __,                     "DANGEROUS GOODS"           ),
	AI( "4322", FNC1   , N,1,1,_, __, __, __, __,                     "AUTH LEAVE"                ),
	AI( "4323", FNC1   , N,1,1,_, __, __, __, __,                     "SIG REQUIRED"              ),
	AI( "4324", FNC1   , N,6,6,_, N,4,4,_, __, __, __,                "NBEF DEL DT."              ),
	AI( "4325", FNC1   , N,6,6,_, N,4,4,_, __, __, __,                "NAFT DEL DT."              ),
	AI( "4326", FNC1   , N,6,6,_, __, __, __, __,                     "REL DATE"                  ),
	AI( "7001", FNC1   , N,13,13,_, __, __, __, __,                   "NSN"                       ),
	AI( "7002", FNC1   , X,1,30,_, __, __, __, __,                    "MEAT CUT"                  ),
	AI( "7003", FNC1   , N,6,6,_, N,4,4,_, __, __, __,                "EXPIRY TIME"               ),
	AI( "7004", FNC1   , N,1,4,_, __, __, __, __,                     "ACTIVE POTENCY"            ),
	AI( "7005", FNC1   , X,1,12,_, __, __, __, __,                    "CATCH AREA"                ),
	AI( "7006", FNC1   , N,6,6,_, __, __, __, __,                     "FIRST FREEZE DATE"         ),
	AI( "7007", FNC1   , N,6,6,_, N,0,6,_, __, __, __,                "HARVEST DATE"              ),
	AI( "7008", FNC1   , X,1,3,_, __, __, __, __,                     "AQUATIC SPECIES"           ),
	AI( "7009", FNC1   , X,1,10,_, __, __, __, __,                    "FISHING GEAR TYPE"         ),
	AI( "7010", FNC1   , X,1,2,_, __, __, __, __,                     "PROD METHOD"               ),
	AI( "7020", FNC1   , X,1,20,_, __, __, __, __,                    "REFURB LOT"                ),
	AI( "7021", FNC1   , X,1,20,_, __, __, __, __,                    "FUNC STAT"                 ),
	AI( "7022", FNC1   , X,1,20,_, __, __, __, __,                    "REV STAT"                  ),
	AI( "7023", FNC1   , X,1,30,_, __, __, __, __,                    "GIAI - ASSEMBLY"           ),
	AI( "7030", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7031", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7032", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7033", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7034", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7035", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7036", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7037", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7038", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7039", FNC1   , N,3,3,_, X,1,27,_, __, __, __,               "PROCESSOR # s"             ),
	AI( "7040", FNC1   , N,1,1,_, X,1,1,_, X,1,1,_, X,1,1,_, __,      "UIC+EXT"                   ),
	AI( "710" , FNC1   , X,1,20,_, __, __, __, __,                    "NHRN PZN"                  ),
	AI( "711" , FNC1   , X,1,20,_, __, __, __, __,                    "NHRN CIP"                  ),
	AI( "712" , FNC1   , X,1,20,_, __, __, __, __,                    "NHRN CN"                   ),
	AI( "713" , FNC1   , X,1,20,_, __, __, __, __,                    "NHRN DRN"                  ),
	AI( "714" , FNC1   , X,1,20,_, __, __, __, __,                    "NHRN AIM"                  ),
	AI( "7230", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7231", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7232", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7233", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7234", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7235", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7236", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7237", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7238", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7239", FNC1   , X,2,2,_, X,1,28,_, __, __, __,               "CERT # s"                  ),
	AI( "7240", FNC1   , X,1,20,_, __, __, __, __,                    "PROTOCOL"                  ),
	AI( "8001", FNC1   , N,4,4,_, N,5,5,_, N,3,3,_, N,1,1,_, N,1,1,_, "DIMENSIONS"                ),
	AI( "8002", FNC1   , X,1,20,_, __, __, __, __,                    "CMT NO."                   ),
	AI( "8003", FNC1   , N,1,1,_, N,13,13,csum, X,0,16,_, __, __,     "GRAI"                      ),
	AI( "8004", FNC1   , X,1,30,_, __, __, __, __,                    "GIAI"                      ),
	AI( "8005", FNC1   , N,6,6,_, __, __, __, __,                     "PRICE PER UNIT"            ),
	AI( "8006", FNC1   , N,14,14,csum, N,4,4,_, __, __, __,           "ITIP"                      ),
	AI( "8007", FNC1   , X,1,34,_, __, __, __, __,                    "IBAN"                      ),
	AI( "8008", FNC1   , N,8,8,_, N,0,4,_, __, __, __,                "PROD TIME"                 ),
	AI( "8009", FNC1   , X,1,50,_, __, __, __, __,                    "OPTSEN"                    ),
	AI( "8010", FNC1   , C,1,30,_, __, __, __, __,                    "CPID"                      ),
	AI( "8011", FNC1   , N,1,12,_, __, __, __, __,                    "CPID SERIAL"               ),
	AI( "8012", FNC1   , X,1,20,_, __, __, __, __,                    "VERSION"                   ),
	AI( "8013", FNC1   , X,1,25,_, __, __, __, __,                    "GMN"                       ),
	AI( "8017", FNC1   , N,18,18,csum, __, __, __, __,                "GSRN - PROVIDER"           ),
	AI( "8018", FNC1   , N,18,18,csum, __, __, __, __,                "GSRN - RECIPIENT"          ),
	AI( "8019", FNC1   , N,1,10,_, __, __, __, __,                    "SRIN"                      ),
	AI( "8020", FNC1   , X,1,25,_, __, __, __, __,                    "REF NO."                   ),
	AI( "8026", FNC1   , N,14,14,csum, N,4,4,_, __, __, __,           "ITIP CONTENT"              ),
	AI( "8110", FNC1   , X,1,70,_, __, __, __, __,                    ""                          ),
	AI( "8111", FNC1   , N,4,4,_, __, __, __, __,                     "POINTS"                    ),
	AI( "8112", FNC1   , X,1,70,_, __, __, __, __,                    ""                          ),
	AI( "8200", FNC1   , X,1,70,_, __, __, __, __,                    "PRODUCT URL"               ),
	AI( "90"  , FNC1   , X,1,30,_, __, __, __, __,                    "INTERNAL"                  ),
	AI( "91"  , FNC1   , X,1,90,_, __, __, __, __,                    "INTERNAL"                  ),
	AI( "92"  , FNC1   , X,1,90,_, __, __, __, __,                    "INTERNAL"                  ),
	AI( "93"  , FNC1   , X,1,90,_, __, __, __, __,                    "INTERNAL"                  ),
	AI( "94"  , FNC1   , X,1,90,_, __, __, __, __,                    "INTERNAL"                  ),
	AI( "95"  , FNC1   , X,1,90,_, __, __, __, __,                    "INTERNAL"                  ),
	AI( "96"  , FNC1   , X,1,90,_, __, __, __, __,                    "INTERNAL"                  ),
	AI( "97"  , FNC1   , X,1,90,_, __, __, __, __,                    "INTERNAL"                  ),
	AI( "98"  , FNC1   , X,1,90,_, __, __, __, __,                    "INTERNAL"                  ),
	AI( "99"  , FNC1   , X,1,90,_, __, __, __, __,                    "INTERNAL"                  ),
};


/*
 * Lookup an AI table entry matching a given AI or matching prefix of given
 * data
 *
 * For an exact AI lookup its length is given. Otherwise 0 length will look for
 * an AI matching a prefix of the given data.
 *
 */
static const struct aiEntry* lookupAIentry(const char *p, size_t ailen) {

	size_t i;
	const struct aiEntry *entry;
	size_t entrylen;

	assert(ailen <= strlen(p));

	// Walk the AI table to find a match, or a prefix match when ailen == 0
	for (i = 0; i < SIZEOF_ARRAY(ai_table); i++) {
		entry = &ai_table[i];
		entrylen = strlen(entry->ai);
		if (ailen != 0 && ailen != entrylen)
			continue;
		if (strncmp(p, entry->ai, ailen > 0 ? ailen : entrylen) == 0)
			break;
	}

	if (i == SIZEOF_ARRAY(ai_table))		// Not found
		return NULL;

	return entry;

}


/*
 * List of DL pkeys
 *
 * These are only used for find the beginning of the DL pathinfo.
 *
 */
static const char* dl_pkeys[] = {
	"00",		// SSCC
	"01",		// GTIN; qualifiers 22,10,21 or 235
	"253",		// GDTI
	"255",		// GCN
	"401",		// GINC
	"402",		// GSIN
	"414",		// LOC NO.; qualifiers=254 or 7040
	"417",		// PARTY; qualifiers=7040
	"8003",		// GRAI
	"8004",		// GIAI; qualifiers=7040
	"8006",		// ITIP; qualifiers=22,10,21
	"8010",		// CPID; qualifiers=8011
	"8013",		// GMN
	"8017",		// GSRN - PROVIDER; qualifiers=8019
	"8018",		// GSRN - RECIPIENT; qualifiers=8019
};

static bool isDLpkey(char* p) {
	size_t i;
	assert(p);
	for (i = 0; i < SIZEOF_ARRAY(dl_pkeys); i++)
		if (strcmp(p, dl_pkeys[i]) == 0)
			return true;
	return false;
}


/*
 *  Validate string between start and end pointers according to rules for an AI
 *
 */
static size_t validate_ai_val(gs1_encoder *ctx, const struct aiEntry *entry, const char *start, const char *end) {

	const struct aiComponent *part;
	size_t i, j;
	char compval[MAX_AI_LEN+1];
	size_t complen;
	linter_t linter;
	const char *p, *r;

	assert(ctx);
	assert(entry);
	assert(start);
	assert(end);
	assert(end >= start);

	DEBUG_PRINT("  Considering AI (%s): %s (first %d characters)\n", entry->ai, start, (int)(end-start));

	p = start;
	r = end;
	if (p == r) {
		sprintf(ctx->errMsg, "AI (%s) data is empty", entry->ai);
		ctx->errFlag = true;
		return 0;
	}

	for (i = 0; i < SIZEOF_ARRAY(ai_table[0].parts); i++) {
		part = &entry->parts[i];
		if (part->cset == cset_none)
			break;

		complen = (size_t)(r-p);	// Until given FNC1 or end...
		if (part->max < r-p)
			complen = part->max;	// ... reduced to max length of component
		strncpy(compval, p, complen);
		compval[complen] = '\0';
		p += complen;

		DEBUG_PRINT("    Validating component: %s\n", compval);

		if (complen < part->min) {
			sprintf(ctx->errMsg, "AI (%s) data is too short", entry->ai);
			ctx->errFlag = true;
			return 0;
		}

		// Run the cset linter
		linter = part->cset == cset_N ? lint_csetNumeric : lint_cset82;
		if (!linter(ctx, entry, compval))
			return 0;

		// Run each additional linter on the component
		for (j = 0; j < SIZEOF_ARRAY(ai_table[0].parts[0].linters); j++) {
			if (!part->linters[j])
				break;
			if (!part->linters[j](ctx, entry, compval))
				return 0;
		}
	}

	return (size_t)(p-start);	// Amount of data that validation consumed

}


// Write to dataStr checking for overflow
#define writeDataStr(v) do {						\
	if (strlen(dataStr) + strlen(v) > MAX_DATA)			\
		goto fail;						\
	strcat(dataStr, v);						\
} while (0)

#define nwriteDataStr(v,l) do {						\
	if (strlen(dataStr) + l > MAX_DATA)				\
		goto fail;						\
	strncat(dataStr, v, l);						\
} while (0)


/*
 * AI length and content check (no "#") used by parsers prior to performing
 * component-based validation since reporting issues such as checksum failure
 * isn't helpful when the AI is too long
 *
 */
static bool aiValLengthContentCheck(gs1_encoder *ctx, const struct aiEntry *entry, const char *aiVal, size_t vallen) {

	size_t i, minlen = 0, maxlen = 0;

	assert(ctx);
	assert(entry);
	assert(aiVal);

	for (i = 0; i < SIZEOF_ARRAY(ai_table[0].parts); i++) {
		minlen += entry->parts[i].min;
		maxlen += entry->parts[i].max;
	}
	if (vallen < minlen) {
		sprintf(ctx->errMsg, "AI (%s) value is too short", entry->ai);
		return false;
	}
	if (vallen > maxlen) {
		sprintf(ctx->errMsg, "AI (%s) value is too long", entry->ai);
		return false;
	}

	// Also forbid data "#" characters at this stage so we don't conflate with FNC1
	if (memchr(aiVal, '#', vallen) != NULL) {
		sprintf(ctx->errMsg, "AI (%s) contains illegal # character", entry->ai);
		return false;
	}

	return true;

}


/*
 * Convert bracketed AI syntax data to regular AI data string with # = FNC1
 *
 */
bool gs1_parseAIdata(gs1_encoder *ctx, const char *aiData, char *dataStr) {

	const char *p, *r;
	char *outval;
	size_t i;
	bool fnc1req = true;
	const struct aiEntry *entry;

	assert(ctx);
	assert(aiData);

	*dataStr = '\0';
	*ctx->errMsg = '\0';
	ctx->errFlag = false;

	DEBUG_PRINT("\nParsing AI data: %s\n", aiData);

	p = aiData;
	while (*p) {

		if (*p++ != '(') goto fail; 			// Expect start of AI
		if (!(r = strchr(p, ')'))) goto fail;		// Find end of A

		entry = lookupAIentry(p, (size_t)(r++ - p));	// Advance to start of data after lookup
		if (entry == NULL) {
			sprintf(ctx->errMsg, "Unrecognised AI: %.4s", p);
			goto fail;
		}

		if (fnc1req)
			writeDataStr("#");			// Write FNC1, if required
		writeDataStr(entry->ai);			// Write AI

		fnc1req = true;					// Determine whether FNC1 required before next AI
		for (i = 0; i < SIZEOF_ARRAY(fixedAIprefixes); i++)
			if (strncmp(fixedAIprefixes[i], entry->ai, 2) == 0)
				fnc1req = false;

		if (!*r) goto fail;				// Fail if message ends after AI and no value

		outval = dataStr + strlen(dataStr);		// Record the current start of the output value

again:

		if ((p = strchr(r, '(')) == NULL)
			p = r + strlen(r);			// Move the pointer to the end if no more AIs

		if (*p != '\0' && *(p-1) == '\\') {		// This bracket is an escaped data character
			nwriteDataStr(r, (size_t)(p-r-1));	// Write up to the escape character
			writeDataStr("(");			// Write the data bracket
			r = p+1;				// And keep going
			goto again;
		}

		nwriteDataStr(r, (size_t)(p-r));		// Write the remainder of the value

		// Perform certain checks at parse time, before processing the
		// components with the linters
		if (!aiValLengthContentCheck(ctx, entry, outval, strlen(outval)))
			goto fail;

	}

	DEBUG_PRINT("Parsing AI data successful: %s\n", dataStr);

	// Now validate the data that we have written
	return gs1_processAIdata(ctx, dataStr);

fail:

	if (*ctx->errMsg == '\0')
		strcpy(ctx->errMsg, "Failed to parse AI data");
	ctx->errFlag = true;

	DEBUG_PRINT("Parsing AI data failed: %s\n", ctx->errMsg);

	*dataStr = '\0';
	return false;

}


static size_t URIunescape(char *out, size_t maxlen, const char *in, const size_t inlen) {

	size_t i, j;
	char hex[3] = { 0 };

	assert(in);
	assert(out);

	for (i = 0, j = 0; i < inlen && j < maxlen; i++, j++) {
		if (i < inlen - 2 && in[i] == '%' && isxdigit(in[i+1]) && isxdigit(in[i+2])) {
			hex[0] = in[i+1];
			hex[1] = in[i+2];
			out[j] = (char)strtoul(hex, NULL, 16);
			i += 2;
		}
		else {
			out[j] = in[i];
		}
	}
	out[j] = '\0';

	return j;

}


/*
 * Convert DL data to regular AI data string with # = FNC1
 *
 * This performs a lightweight parse, sufficient for extracting the AIs for
 * validation and HRI purposes.
 *
 * It does not validate the structure of the DL URI nor the data relationships
 * between the extracted AIs.
 *
 * Extraction using convenience strings for GS1 keys are not supported.
 *
 */
bool gs1_parseDLuri(gs1_encoder *ctx, char *dlData, char *dataStr) {

	char *p, *r, *e;
	char *pi = NULL;	// Path info
	char *qp = NULL;	// Query params
	char *fr = NULL;	// Fragment
	char *dp = NULL;	// DL path info
	bool ret;
	size_t i;
	size_t vallen;
	bool fnc1req = true;
	const struct aiEntry *entry;
	char aival[MAX_AI_LEN+1];	// Unescaped AI value

	assert(ctx);
	assert(dlData);

	*dataStr = '\0';
	*ctx->errMsg = '\0';
	ctx->errFlag = false;

	DEBUG_PRINT("\nParsing DL data: %s\n", dlData);

	p = dlData;

	if (strspn(p, uriCharacters) != strlen(p)) {
		strcpy(ctx->errMsg, "URI contains illegal characters");
		goto fail;
	}

	if (strlen(p) >= 8 && strncmp(p, "https://", 8) == 0)
		p += 8;
	else if (strlen(p) >= 7 && strncmp(p, "http://", 7) == 0)
		p += 7;
	else {
		strcpy(ctx->errMsg, "Scheme must be http:// or https://");
		goto fail;
	}

	DEBUG_PRINT("  Schema: %.*s\n", p-dlData, dlData);

	if (((r = strchr(p, '/')) == NULL) || r-p < 1) {
		strcpy(ctx->errMsg, "URI must contain a domain and path info");
		goto fail;
	}

	DEBUG_PRINT("  Domain: %.*s\n", r-p, p);

	pi = p = r;					// Skip the domain name

	// Query parameter marker delimits end of path info
	if ((qp = strchr(pi, '?')) != NULL)
		*qp++ = '\0';

	DEBUG_PRINT("  Path info: %s\n", pi);

	// Search backwards from the end of the path info looking for an
	// "/AI/value" pair where AI is a DL primary key
	while ((r = strrchr(pi, '/')) != NULL) {

		*p = '/';				// Restore original pair separator
							// Clobbers first character of path
							// info on first iteration

		// Find start of AI
		*r = '\0';				// Chop off value
		p = strrchr(pi, '/'); 			// Beginning of AI
		*r = '/';				// Restore original AI/value separator
		if (!p)					// At beginning of path
			break;

		DEBUG_PRINT("      %s\n", p);

		entry = lookupAIentry(p+1, (size_t)(r-p-1));
		if (!entry)
			break;

		if (isDLpkey(entry->ai)) {		// Found root of DL path info
			dp = p;
			break;
		}

		*p = '\0';

	}

	if (!dp) {
		strcpy(ctx->errMsg, "No GS1 DL keys found in path info");
		goto fail;
	}

	DEBUG_PRINT("  Stem: %.*s\n", dp-dlData, dlData);

	DEBUG_PRINT("  DL path info: %s\n", dp);

	// Process each AI value pair in the DL path info
	p = dp;
	while (*p) {
		assert(*p == '/');
		p++;
		r = strchr(p, '/');
		assert(r);
		entry = lookupAIentry(p, (size_t)(r-p));	// Known to exist
		assert(entry);

		if ((p = strchr(++r, '/')) == NULL)
			p = r + strlen(r);

;		// Reverse percent encoding
		if ((vallen = URIunescape(aival, MAX_AI_LEN, r, (size_t)(p-r))) == 0) {
			sprintf(ctx->errMsg, "Decoded AI (%s) from DL path info too long", entry->ai);
			goto fail;
		}

		// Special handling of AI (01) to pad up to a GTIN-14
		if (strcmp(entry->ai, "01") == 0 &&
		    (vallen == 13 || vallen == 12 || vallen == 8)) {
			for (i = 0; i <= 13; i++)
				aival[13-i] = vallen >= i+1 ? aival[vallen-i-1] : '0';
			aival[14] = '\0';
			vallen = 14;
		}

		DEBUG_PRINT("    Extracted: (%s) %.*s\n", entry->ai, vallen, aival);

		if (fnc1req)
			writeDataStr("#");			// Write FNC1, if required
		writeDataStr(entry->ai);			// Write AI

		fnc1req = true;					// Determine whether FNC1 required before next AI
		for (i = 0; i < SIZEOF_ARRAY(fixedAIprefixes); i++)
			if (strncmp(fixedAIprefixes[i], entry->ai, 2) == 0)
				fnc1req = false;

		nwriteDataStr(aival, vallen);			// Write value

		// Perform certain checks at parse time, before processing the
		// components with the linters
		if (!aiValLengthContentCheck(ctx, entry, aival, vallen))
			goto fail;

	}

	// Fragment character delimits end of the query parameters
	if (qp && ((fr = strchr(qp, '#')) != NULL))
		*fr++ = '\0';

	if (qp)
		DEBUG_PRINT("  Query params: %s\n", qp);

	p = qp;
	while (p && *p) {

		while (*p == '&')				// Jump any & separators
			p++;
		if ((r = strchr(p, '&')) == NULL)
			r = p + strlen(p);			// Value-pair finishes at end of data

		// Discard parameters with no value and unknown AIs
		if ((e = memchr(p, '=', (size_t)(r-p))) == NULL) {
			DEBUG_PRINT("    Skipped singleton:   %.*s\n", r-p, p);
			p = r;
			continue;
		}

		// Numeric-only query parameters not matching an AI aren't allowed
		entry = NULL;
		if (gs1_allDigits((uint8_t*)p, (size_t)(e-p)) && (entry = lookupAIentry(p, (size_t)(e-p))) == NULL) {
			sprintf(ctx->errMsg, "Unknown AI (%.*s) in query parameters", (int)(e-p), p);
			goto fail;
		}

		// Skip non-numeric query parameters
		if (!entry) {
			DEBUG_PRINT("    Skipped:   %.*s\n", r-p, p);
			p = r;
			continue;
		}

		// Reverse percent encoding
		e++;
		if ((vallen = URIunescape(aival, MAX_AI_LEN, e, (size_t)(r-e))) == 0) {
			sprintf(ctx->errMsg, "Decoded AI (%s) value from DL query params too long", entry->ai);
			goto fail;
		}

		// Special handling of AI (01) to pad up to a GTIN-14
		if (strcmp(entry->ai, "01") == 0 &&
		    (vallen == 13 || vallen == 12 || vallen == 8)) {
			for (i = 0; i <= 13; i++)
				aival[13-i] = vallen >= i+1 ? aival[vallen-i-1] : '0';
			aival[14] = '\0';
			vallen = 14;
		}

		DEBUG_PRINT("    Extracted: (%s) %.*s\n", entry->ai, vallen, aival);

		if (fnc1req)
			writeDataStr("#");			// Write FNC1, if required
		writeDataStr(entry->ai);			// Write AI

		fnc1req = true;					// Determine whether FNC1 required before next AI
		for (i = 0; i < SIZEOF_ARRAY(fixedAIprefixes); i++)
			if (strncmp(fixedAIprefixes[i], entry->ai, 2) == 0)
				fnc1req = false;

		nwriteDataStr(aival, vallen);			// Write value

		// Perform certain checks at parse time, before processing the
		// components with the linters
		if (!aiValLengthContentCheck(ctx, entry, aival, vallen))
			goto fail;

		p = r;

	}

	DEBUG_PRINT("Parsing DL data successful: %s\n", dataStr);

	// Now validate the data that we have written
	ret = gs1_processAIdata(ctx, dataStr);

out:

	if (qp)			// Restore original query parameter delimeter
		*(qp-1) = '?';

	if (fr)			// Restore original fragment delimieter
		*(fr-1) = '#';

	return ret;

fail:

	if (*ctx->errMsg == '\0')
		strcpy(ctx->errMsg, "Failed to parse DL data");
	ctx->errFlag = true;

	DEBUG_PRINT("Parsing DL data failed: %s\n", ctx->errMsg);

	*dataStr = '\0';
	ret = false;
	goto out;

}


/*
 *  Validate regular AI data ("#...") and extract AIs
 *
 */
bool gs1_processAIdata(gs1_encoder *ctx, const char *dataStr) {

	const char *p, *r;
	size_t vallen;
	const struct aiEntry *entry;

	assert(ctx);
	assert(dataStr);

	*ctx->errMsg = '\0';
	ctx->errFlag = false;

	p = dataStr;

	// Ensure FNC1 in first
	if (!*p || *p++ != '#') {
		strcpy(ctx->errMsg, "Missing FNC1 in first position");
		ctx->errFlag = true;
		return false;
	}

	// Must have some AI data
	if (!*p) {
		strcpy(ctx->errMsg, "The AI data is empty");
		ctx->errFlag = true;
		return false;
	}

	while (*p) {

		// Find AI that matches a prefix of our data
		if ((entry = lookupAIentry(p, 0)) == NULL) {
			sprintf(ctx->errMsg, "Unrecognised AI: %.4s", p);
			ctx->errFlag = true;
			return false;
		}

		p += strlen(entry->ai);

		// r points to the next FNC1 or end of string...
		if ((r = strchr(p, '#')) == NULL)
			r = p + strlen(p);

		// Validate and return how much was consumed
		if ((vallen = validate_ai_val(ctx, entry, p, r)) == 0)
			return false;

		// Add to the aiData
		if (ctx->numAIs < MAX_AIS) {
			ctx->aiData[ctx->numAIs].aiEntry = entry;
			ctx->aiData[ctx->numAIs].value = p;
			ctx->aiData[ctx->numAIs].vallen = (uint8_t)vallen;
			ctx->numAIs++;
		} else {
			strcpy(ctx->errMsg, "Too many AIs");
			ctx->errFlag = true;
			return false;
		}

		// After AIs requiring FNC1, we expect to find an FNC1 or be at the end
		p += vallen;
		if (entry->fnc1 && *p != '#' && *p != '\0') {
			sprintf(ctx->errMsg, "AI (%s) data is too long", entry->ai);
			ctx->errFlag = true;
			return false;
		}

		// Skip FNC1, even at end of fixed-length AIs
		if (*p == '#')
			p++;

	}

	return true;

}


// Validate and set the parity digit
bool gs1_validateParity(uint8_t *str) {

	int weight;
	int parity = 0;

	assert(*str);

	weight = strlen((char*)str) % 2 == 0 ? 3 : 1;
	while (*(str+1)) {
		parity += weight * (*str++ - '0');
		weight = 4 - weight;
	}
	parity = (10 - parity%10) % 10;

	if (parity + '0' == *str) return true;

	*str = (uint8_t)(parity + '0');		// Recalculate
	return false;

}


bool gs1_allDigits(const uint8_t *str, size_t len) {

	size_t i;

	assert(str);

	if (!len)
		len = strlen((char *)str);

	for (i = 0; i < len; i++) {
		if (str[i] < '0' || str[i] >'9')
			return false;
	}
	return true;

}



#ifdef UNIT_TESTS

#define TEST_NO_MAIN
#include "acutest.h"


void test_gs1_lookupAIentry(void) {

	TEST_CHECK(strcmp(lookupAIentry("01",     2)->ai, "01") == 0);		// Exact lookup, data following
	TEST_CHECK(strcmp(lookupAIentry("011234", 2)->ai, "01") == 0);		// Exact lookup, data following
	TEST_CHECK(strcmp(lookupAIentry("011234", 0)->ai, "01") == 0);		// Prefix lookup, data following
	TEST_CHECK(strcmp(lookupAIentry("8012",   0)->ai, "8012") == 0);	// Prefix lookup, data following

	TEST_CHECK(lookupAIentry("2345XX", 4) == NULL);				// No such AI (2345)
	TEST_CHECK(lookupAIentry("234XXX", 3) == NULL);				// No such AI (234)
	TEST_CHECK(lookupAIentry("23XXXX", 2) == NULL);				// No such AI (23)
	TEST_CHECK(lookupAIentry("2XXXXX", 1) == NULL);				// No such AI (2)
	TEST_CHECK(lookupAIentry("XXXXXX", 0) == NULL);				// No matching prefix
	TEST_CHECK(lookupAIentry("234567", 0) == NULL);				// No matching prefix

	TEST_CHECK(strcmp(lookupAIentry("235XXX", 0)->ai, "235") == 0);		// Matching prefix
	TEST_CHECK(lookupAIentry("235XXX", 2) == NULL);				// No such AI (23), even though data starts 235
	TEST_CHECK(lookupAIentry("235XXX", 1) == NULL);				// No such AI (2), even though data starts 235

	TEST_CHECK(strcmp(lookupAIentry("37123", 2)->ai, "37") == 0);		// Exact lookup
	TEST_CHECK(lookupAIentry("37123", 3) == NULL);				// No such AI (371), even though there is AI (37)
	TEST_CHECK(lookupAIentry("37123", 1) == NULL);				// No such AI (3), even though there is AI (37)

}


static void test_parseAIdata(gs1_encoder *ctx, const bool should_succeed, const char *aiData, const char* expect) {

	char out[256];
	char casename[256];

	sprintf(casename, "%s => %s", aiData, expect);
	TEST_CASE(casename);

	ctx->numAIs = 0;
	TEST_CHECK(gs1_parseAIdata(ctx, aiData, out) ^ !should_succeed);
	if (should_succeed)
		TEST_CHECK(strcmp(out, expect) == 0);
	TEST_MSG("Given: %s; Got: %s; Expected: %s; Err: %s", aiData, out, expect, ctx->errMsg);

}


/*
 *  Convert a bracketed AI string to a regular AI string "#..."
 *
 */
void test_gs1_parseAIdata(void) {

	gs1_encoder* ctx = gs1_encoder_init(NULL);

	test_parseAIdata(ctx, true,  "(01)12345678901231", "#0112345678901231");
	test_parseAIdata(ctx, true,  "(10)12345", "#1012345");
	test_parseAIdata(ctx, true,  "(01)12345678901231(10)12345", "#01123456789012311012345");	// No FNC1 after (01)
	test_parseAIdata(ctx, true,  "(3100)123456(10)12345", "#31001234561012345");			// No FNC1 after (3100)
	test_parseAIdata(ctx, true,  "(10)12345(11)991225", "#1012345#11991225");			// FNC1 after (10)
	test_parseAIdata(ctx, true,  "(3900)12345(11)991225", "#390012345#11991225");			// FNC1 after (3900)
	test_parseAIdata(ctx, true,  "(10)12345\\(11)991225", "#1012345(11)991225");			// Escaped bracket
	test_parseAIdata(ctx, true,  "(10)12345\\(", "#1012345(");					// At end if fine

	test_parseAIdata(ctx, false, "(10)(11)98765", "");						// Value must not be empty
	test_parseAIdata(ctx, false, "(10)12345(11)", "");						// Value must not be empty
	test_parseAIdata(ctx, false, "(1A)12345", "");							// AI must be numeric
	test_parseAIdata(ctx, false, "1(12345", "");							// Must start with AI
	test_parseAIdata(ctx, false, "12345", "");							// Must start with AI
	test_parseAIdata(ctx, false, "()12345", "");							// AI too short
	test_parseAIdata(ctx, false, "(1)12345", "");							// AI too short
	test_parseAIdata(ctx, false, "(12345)12345", "");						// AI too long
	test_parseAIdata(ctx, false, "(15", "");							// AI must terminate
	test_parseAIdata(ctx, false, "(1", "");								// AI must terminate
	test_parseAIdata(ctx, false, "(", "");								// AI must terminate
	test_parseAIdata(ctx, false, "(01)123456789012312(10)12345", "");				// Fixed-length AI too long
	test_parseAIdata(ctx, false, "(10)12345#", "");							// Reject "#": Conflated with FNC1
	test_parseAIdata(ctx, false, "(17)9(90)217", "");						// Should not parse to #17990217

	gs1_encoder_free(ctx);

}


static void test_parseDLuri(gs1_encoder *ctx, bool should_succeed, const char *dlData, const char* expect) {

	char in[256];
	char out[256];
	char casename[256];

	sprintf(casename, "%s => %s", dlData, expect);
	TEST_CASE(casename);

	ctx->numAIs = 0;
	strcpy(in, dlData);
	TEST_CHECK(gs1_parseDLuri(ctx, in, out) ^ !should_succeed);
	TEST_MSG("Err: %s", ctx->errMsg);
	if (should_succeed)
		TEST_CHECK(strcmp(out, expect) == 0);
	TEST_MSG("Given: %s; Got: %s; Expected: %s; Err: %s", dlData, out, expect, ctx->errMsg);

}


/*
 *  Convert a DL URI to a regular AI string "#..."
 *
 */
void test_gs1_parseDLuri(void) {

	gs1_encoder* ctx = gs1_encoder_init(NULL);

	test_parseDLuri(ctx, false,  "", "");
	test_parseDLuri(ctx, false,  "ftp://", "");
	test_parseDLuri(ctx, false,  "http://", "");
	test_parseDLuri(ctx, false,  "http:///", "");			// No domain
	test_parseDLuri(ctx, false,  "http://a", "");			// No path info
	test_parseDLuri(ctx, false,  "http://a/", "");			// Pathelogical minimal domain but no AI info

	test_parseDLuri(ctx, true,					// http
		"http://a/00/006141411234567890",
		"#00006141411234567890");

	test_parseDLuri(ctx, true,					// https
		"https://a/00/006141411234567890",
		"#00006141411234567890");

	test_parseDLuri(ctx, false,					// No domain
		"https://00/006141411234567890",
		"");

	test_parseDLuri(ctx, true,
		"https://a/stem/00/006141411234567890",
		"#00006141411234567890");

	test_parseDLuri(ctx, true,
		"https://a/more/stem/00/006141411234567890",
		"#00006141411234567890");

	test_parseDLuri(ctx, true,					// Fake AI in stem, stop at rightmost key
		"https://a/00/faux/00/006141411234567890",
		"#00006141411234567890");

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333",
		"#0112312312312333");

	test_parseDLuri(ctx, true,					// GTIN-13 -> GTIN-14
		"https://a/01/2112345678900",
		"#0102112345678900");

	test_parseDLuri(ctx, true,					// GTIN-12 -> GTIN-14
		"https://a/01/416000336108",
		"#0100416000336108");

	test_parseDLuri(ctx, true,					// GTIN-8 -> GTIN-14
		"https://a/01/02345673",
		"#0100000002345673");

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333/22/TEST/10/ABC/21/XYZ",
		"#011231231231233322TEST#10ABC#21XYZ");

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333/235/TEST",
		"#0112312312312333235TEST");

	test_parseDLuri(ctx, true,
		"https://a/253/1231231231232",
		"#2531231231231232");

	test_parseDLuri(ctx, true,
		"https://a/253/1231231231232TEST5678901234567",
		"#2531231231231232TEST5678901234567");

	test_parseDLuri(ctx, false,
		"https://a/253/1231231231232TEST56789012345678", "");	// Too long N13 X0..17

	test_parseDLuri(ctx, true,
		"https://a/8018/123456789012345675/8019/123",
		"#8018123456789012345675#8019123");

	test_parseDLuri(ctx, false,
		"https://a/stem/00/006141411234567890/", ""); 		// Can't end in slash

	test_parseDLuri(ctx, true,
		"https://a/stem/00/006141411234567890?99=ABC",		// Query params; no FNC1 req after pathinfo
		 "#0000614141123456789099ABC");

	test_parseDLuri(ctx, true,
		"https://a/stem/401/12345678?99=ABC",			// Query params; FNC1 req after pathinfo
		 "#40112345678#99ABC");

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333?99=ABC&98=XYZ",
		"#011231231231233399ABC#98XYZ");

	test_parseDLuri(ctx, false,
		"https://a/01/12312312312333?99=ABC&999=faux", "");	// Non-AI, numeric-only query param

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333?&&&99=ABC&&&&&&98=XYZ&&&",	// Extraneous query param separators
		"#011231231231233399ABC#98XYZ");

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333?99=ABC&unknown=666&98=XYZ",
		"#011231231231233399ABC#98XYZ");

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333?99=ABC&singleton&98=XYZ",
		"#011231231231233399ABC#98XYZ");

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333?singleton&99=ABC&98=XYZ",
		"#011231231231233399ABC#98XYZ");

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333?99=ABC&98=XYZ&singleton",
		"#011231231231233399ABC#98XYZ");

	test_parseDLuri(ctx, true,
		"https://a/01/12312312312333/22/ABC%2d123?99=ABC&98=XYZ%2f987",	// Percent escaped values
		"#011231231231233322ABC-123#99ABC#98XYZ/987");


	/*
	 * Examples for DL specification
	 *
	 */

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/01/09520123456788",
		"#0109520123456788");

	test_parseDLuri(ctx, true,
		"https://brand.example.com/01/9520123456788",
		"#0109520123456788");

	test_parseDLuri(ctx, true,
		"https://brand.example.com/some-extra/pathinfo/01/9520123456788",
		"#0109520123456788");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/01/09520123456788/22/2A",
		"#0109520123456788222A");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/01/09520123456788/10/ABC123",
		"#010952012345678810ABC123");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/01/09520123456788/21/12345",
		"#01095201234567882112345");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/01/09520123456788/10/ABC1/21/12345?17=180426",
		"#010952012345678810ABC1#2112345#17180426");
	// Specification sorts (17) before (10) and (21):
	//   "#01095201234567881718042610ABC1#2112345"

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/01/09520123456788?3103=000195",
		"#01095201234567883103000195");

	test_parseDLuri(ctx, true,
		"https://example.com/01/9520123456788?3103=000195&3922=0299&17=201225",
		"#0109520123456788310300019539220299#17201225");

	test_parseDLuri(ctx, true,
		"https://example.com/01/9520123456788?3103=000195&3922=0299&17=201225",
		"#0109520123456788310300019539220299#17201225");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/01/9520123456788?3103=000195&3922=0299&17=201225",
		"#0109520123456788310300019539220299#17201225");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/01/9520123456788?17=201225&3103=000195&3922=0299",
		"#010952012345678817201225310300019539220299");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/00/952012345678912345",
		"#00952012345678912345");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/00/952012345678912345?02=09520123456788&37=25&10=ABC123",
		"#0095201234567891234502095201234567883725#10ABC123");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/414/9520123456788",
		"#4149520123456788");

	test_parseDLuri(ctx, true,
		"https://id.gs1.org/414/9520123456788/254/32a%2Fb",
		"#414952012345678825432a/b");

	test_parseDLuri(ctx, true,
		"https://example.com/8004/9520614141234567?01=9520123456788",
		"#80049520614141234567#0109520123456788");

	gs1_encoder_free(ctx);

}


static void test_processAIdata(gs1_encoder *ctx, const bool should_succeed, const char *dataStr) {

	char casename[256];

	sprintf(casename, "%s", dataStr);
	TEST_CASE(casename);

	TEST_CHECK(gs1_processAIdata(ctx, dataStr) ^ !should_succeed);
	TEST_MSG(ctx->errMsg);

}


void test_gs1_processAIdata(void) {

	gs1_encoder* ctx = gs1_encoder_init(NULL);

	test_processAIdata(ctx, false, "");						// No FNC1 in first position
	test_processAIdata(ctx, false, "991234");					// No FNC1 in first position
	test_processAIdata(ctx, false, "#");						// FNC1 in first but no AIs
	test_processAIdata(ctx, false, "#891234");					// No such AI

	test_processAIdata(ctx, true,  "#991234");

	test_processAIdata(ctx, false, "#99~ABC");					// Bad CSET82 character
 	test_processAIdata(ctx, false, "#99ABC~");					// Bad CSET82 character

	test_processAIdata(ctx, true,  "#0112345678901231");				// N14, no FNC1 required
	test_processAIdata(ctx, false, "#01A2345678901231");				// Bad numeric character
	test_processAIdata(ctx, false, "#011234567890123A");				// Bad numeric character
	test_processAIdata(ctx, false, "#0112345678901234");				// Incorrect check digit (csum linter)
	test_processAIdata(ctx, false, "#011234567890123");				// Too short
	test_processAIdata(ctx, false, "#01123456789012312");				// No such AI (2). Can't be "too long" since FNC1 not required

	test_processAIdata(ctx, true,  "#0112345678901231#");				// Tolerate superflous FNC1
	test_processAIdata(ctx, false, "#011234567890123#");				// Short, with superflous FNC1
	test_processAIdata(ctx, false, "#01123456789012345#");				// Long, with superflous FNC1 (no following AIs)
	test_processAIdata(ctx, false, "#01123456789012345#991234");			// Long, with superflous FNC1 and meaningless AI (5#..)

	test_processAIdata(ctx, true,  "#0112345678901231991234");			// Fixed-length, run into next AI (01)...(99)...
	test_processAIdata(ctx, true,  "#0112345678901231#991234");			// Tolerate superflous FNC1

	test_processAIdata(ctx, true,  "#2421");					// N1..6; FNC1 required
	test_processAIdata(ctx, true,  "#24212");
	test_processAIdata(ctx, true,  "#242123");
	test_processAIdata(ctx, true,  "#2421234");
	test_processAIdata(ctx, true,  "#24212345");
	test_processAIdata(ctx, true,  "#242123456");
	test_processAIdata(ctx, true,  "#242123456#10ABC123");				// Limit, then following AI
	test_processAIdata(ctx, true,  "#242123456#");					// Tolerant of FNC1 at end of data
	test_processAIdata(ctx, false, "#2421234567");					// Data too long

	test_processAIdata(ctx, true,  "#81111234");					// N4; FNC1 required
	test_processAIdata(ctx, false, "#8111123");					// Too short
	test_processAIdata(ctx, false, "#811112345");					// Too long
	test_processAIdata(ctx, true,  "#81111234#10ABC123");				// Followed by another AI

	test_processAIdata(ctx, true,  "#800112341234512398");				// N4-5-3-1-1; FNC1 required
	test_processAIdata(ctx, false, "#80011234123451239");				// Too short
	test_processAIdata(ctx, false, "#8001123412345123981");				// Too long
	test_processAIdata(ctx, true,  "#800112341234512398#0112345678901231");
	test_processAIdata(ctx, false, "#80011234123451239#0112345678901231");		// Too short
	test_processAIdata(ctx, false, "#8001123412345123981#01123456789012312");	// Too long

	test_processAIdata(ctx, true,  "#800302112345678900ABC");			// N1 N13,csum X0..16; FNC1 required
	test_processAIdata(ctx, false, "#800302112345678901ABC");			// Bad check digit on N13 component
	test_processAIdata(ctx, true,  "#800302112345678900");				// Empty final component
	test_processAIdata(ctx, true,  "#800302112345678900#10ABC123");			// Empty final component and following AI
	test_processAIdata(ctx, true,  "#800302112345678900ABCDEFGHIJKLMNOP");		// Empty final component and following AI
	test_processAIdata(ctx, false, "#800302112345678900ABCDEFGHIJKLMNOPQ");		// Empty final component and following AI

	test_processAIdata(ctx, true,  "#7230121234567890123456789012345678");		// X2 X1..28; FNC1 required
	test_processAIdata(ctx, false, "#72301212345678901234567890123456789");		// Too long
	test_processAIdata(ctx, true,  "#7230123");					// Shortest
	test_processAIdata(ctx, false, "#723012");					// Too short

	gs1_encoder_free(ctx);

}


void test_gs1_validateParity(void) {

	char good_gtin14[] = "24012345678905";
	char bad_gtin14[]  = "24012345678909";
	char good_gtin13[] = "2112233789657";
	char bad_gtin13[]  = "2112233789658";
	char good_gtin12[] = "416000336108";
	char bad_gtin12[]  = "416000336107";
	char good_gtin8[]  = "02345680";
	char bad_gtin8[]   = "02345689";

	TEST_CHECK(gs1_validateParity((uint8_t*)good_gtin14));
	TEST_CHECK(!gs1_validateParity((uint8_t*)bad_gtin14));
	TEST_CHECK(bad_gtin14[13] == '5');		// Recomputed

	TEST_CHECK(gs1_validateParity((uint8_t*)good_gtin13));
	TEST_CHECK(!gs1_validateParity((uint8_t*)bad_gtin13));
	TEST_CHECK(bad_gtin13[12] == '7');		// Recomputed

	TEST_CHECK(gs1_validateParity((uint8_t*)good_gtin12));
	TEST_CHECK(!gs1_validateParity((uint8_t*)bad_gtin12));
	TEST_CHECK(bad_gtin12[11] == '8');		// Recomputed

	TEST_CHECK(gs1_validateParity((uint8_t*)good_gtin8));
	TEST_CHECK(!gs1_validateParity((uint8_t*)bad_gtin8));
	TEST_CHECK(bad_gtin8[7] == '0');		// Recomputed

}


static void test_gs1_doURIunescape(const char *in, const char *expect) {

	char out[MAX_AI_LEN+1];

	TEST_CHECK(URIunescape(out, MAX_AI_LEN, in, strlen(in)) == strlen(expect));
	TEST_CHECK(strcmp(out, expect) == 0);
	TEST_MSG("Given: %s; Got: %s; Expected: %s", in, out, expect);

}


void test_gs1_URIunescape(void) {

	char out[MAX_AI_LEN+1];

	test_gs1_doURIunescape("", "");
	test_gs1_doURIunescape("test", "test");
	test_gs1_doURIunescape("%20", " ");
	test_gs1_doURIunescape("%20AB", " AB");
	test_gs1_doURIunescape("A%20B", "A B");
	test_gs1_doURIunescape("AB%20", "AB ");
	test_gs1_doURIunescape("ABC%2", "ABC%2");		// Off end
	test_gs1_doURIunescape("ABCD%", "ABCD%");
	test_gs1_doURIunescape("A%20%20B", "A  B");		// Run together
	test_gs1_doURIunescape("A%01B", "A" "\x01" "B");	// "Minima", we check \0 below
	test_gs1_doURIunescape("A%ffB", "A" "\xFF" "B");	// Maxima
	test_gs1_doURIunescape("A%FfB", "A" "\xFF" "B");	// Case mixing
	test_gs1_doURIunescape("A%fFB", "A" "\xFF" "B");	// Case mixing
	test_gs1_doURIunescape("A%FFB", "A" "\xFF" "B");	// Case mixing
	test_gs1_doURIunescape("A%4FB", "AOB");
	test_gs1_doURIunescape("A%4fB", "AOB");
	test_gs1_doURIunescape("A%4gB", "A%4gB");		// Non hex digit
	test_gs1_doURIunescape("A%4GB", "A%4GB");		// Non hex digit
	test_gs1_doURIunescape("A%g4B", "A%g4B");		// Non hex digit
	test_gs1_doURIunescape("A%G4B", "A%G4B");		// Non hex digit

	// Check that \0 is sane, although we are only working with strings
	TEST_CHECK(URIunescape(out, MAX_AI_LEN, "A%00B", 5) == 3);
	TEST_CHECK(memcmp(out, "A" "\x00" "B", 4) == 0);

	// Truncated input
	TEST_CHECK(URIunescape(out, MAX_AI_LEN, "ABCD", 2) == 2);
	TEST_CHECK(memcmp(out, "AB", 3) == 0);			// Includes \0

	// Truncated output
	TEST_CHECK(URIunescape(out, 2, "ABCD", 4) == 2);
	TEST_CHECK(memcmp(out, "AB", 3) == 0);			// Includes \0

}


#endif  /* UNIT_TESTS */

