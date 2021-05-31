/**
 * GS1 Barcode Encoder Library
 *
 * @file gs1encoders.h
 * @author GS1 AISBL
 *
 * \copyright Copyright (c) 2000-2021 GS1 AISBL.
 *
 * @licenseblock{License}
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
 * @endlicenseblock
 *
 */

#ifndef GS1_ENCODERS_H
#define GS1_ENCODERS_H

/// \cond
#include <stdbool.h>
#include <stddef.h>
/// \endcond


// Decorator for public API functions that we export from a DLL
#ifdef _WIN32
#  define GS1_ENCODERS_API __declspec(dllexport)
#else
#  define GS1_ENCODERS_API
#endif


#ifdef __cplusplus
extern "C" {
#endif


/// enum to select a symbology.
enum symbologies {
	gs1_encoder_sNONE = -1,		///< None defined
	gs1_encoder_sRSS14,		///< GS1 DataBar Omnidirectional
	gs1_encoder_sRSS14T,		///< GS1 DataBar Truncated
	gs1_encoder_sRSS14S,		///< GS1 DataBar Stacked
	gs1_encoder_sRSS14SO,		///< GS1 DataBar Stacked Omnidirectional
	gs1_encoder_sRSSLIM,		///< GS1 DataBar Limited
	gs1_encoder_sRSSEXP,		///< GS1 DataBar Expanded (Stacked)
	gs1_encoder_sUPCA,		///< UPC-A
	gs1_encoder_sUPCE,		///< UPC-E
	gs1_encoder_sEAN13,		///< EAN-13
	gs1_encoder_sEAN8,		///< EAN-8
	gs1_encoder_sUCC128_CCA,	///< GS1-128 with CC-A or CC-B
	gs1_encoder_sUCC128_CCC,	///< GS1-128 with CC-C
	gs1_encoder_sQR,		///< (GS1) QR Code
	gs1_encoder_sDM,		///< (GS1) Data Matrix
	gs1_encoder_sNUMSYMS,		///< Value is the number of symbologies
};


/// enum to select the output format.
enum formats {
	gs1_encoder_dBMP = 0,		///< BMP format
	gs1_encoder_dTIF = 1,		///< TIFF format
	gs1_encoder_dRAW = 2,		///< TIFF, without header (1-bit per pixel matrix with byte-aligned rows)
};


/// enum to select QR Code error correction level.
enum qrEClevel {
	gs1_encoder_qrEClevelL = 1,	///< Low error correction (7% damage recovery)
	gs1_encoder_qrEClevelM,		///< Medium error correction (15% damage recovery)
	gs1_encoder_qrEClevelQ,		///< Quartile error correction (25% damage recovery)
	gs1_encoder_qrEClevelH,		///< High error correction (30% damage recovery)
};


/**
 * @brief A gs1_encoder context.
 *
 * This is an opaque struct that represents an encoder context.
 *
 * A context is an "instance" of the library and maintains all state required
 * for that instance. Any number of instances of the library can be created,
 * each operating seperately and equivalently to all others.
 *
 * This library does not introduce and global variables. In general, all state
 * is maintained in instances of the gs1_encoder struct and this state should
 * only be modified using the public API functions provided by this library
 * (decorated with GS1_ENCODERS_API).
 *
 * A context is created by calling gs1_encoder_init() and destroyed by calling
 * gs1_encoder_free().
 *
 * \note
 * This struct is deliberately opaque and it's layout should be assumed to vary
 * between releases of the library and between builds create with different
 * options.
 *
 * \note
 * The library is thread-safe provided that each thread operates on its own
 * instance of the library.
 *
 */
typedef struct gs1_encoder gs1_encoder;


/**
 * @brief Get the version string of the library.
 *
 * This is typically the build date.
 *
 * @param[in,out] ctx instance of ::gs1_encoder
 * @return pointer to a string containing the version of the library
 */
GS1_ENCODERS_API char* gs1_encoder_getVersion(gs1_encoder *ctx);


/**
 * @brief Find the memory storage requirements for an instance of ::gs1_encoder.
 *
 * For embedded systems it may be desirable to provide a pre-allocated buffer
 * to a new context for storage purposes, rather than have the instance
 * malloc() it's own storage.
 *
 * This function provides the minimum size required for such a buffer.
 *
 * Example of allocating our own heap storage:
 *
 * \code{.c}
 * gs1_encoder *ctx;
 * size_t mem = gs1_encoder_instanceSize();
 * void *heap = malloc(mem);
 * assert(heap);
 * ctx = gs1_encoder_init(heap);
 * \endcode
 *
 * Example of using static storage:
 *
 * \code{.c}
 * static uint8_t prealloc[65536];
 * ...
 * void myFunc(void) {
 * 	gs1_encoder *ctx;
 * 	size_t mem = gs1_encoder_instanceSize();
 * 	assert(sizeof(prealloc) <= mem);
 * 	ctx = gs1_encoder_init((void *)prealloc);
 * 	...
 * }
 * \endcode
 *
 * @return memory required to hold a context instance
 */
GS1_ENCODERS_API size_t gs1_encoder_instanceSize(void);


/**
 * @brief Get the maximum filename length that can be used for input and output.
 *
 * This is an implementation limit that may be lowered for systems with limited
 * memory by rebuilding the library.
 *
 * @see gs1_encoder_setDataFile()
 *
 * @return maximum length of a filename
 */
GS1_ENCODERS_API int gs1_encoder_getMaxFilenameLength(void);


/**
 * @brief Get the maximum size of the input data buffer for barcode message content.
 *
 * This is an implementation limit that may be lowered for systems with limited
 * memory by rebuilding the library.
 *
 * \note
 * Each barcode symbology has its own data capacity that may be somewhat less
 * than this maximum size.
 *
 * @see gs1_encoder_setDataStr()
 *
 * @return maximum number bytes that can be supplied for encoding
 */
GS1_ENCODERS_API int gs1_encoder_getMaxInputBuffer(void);


/**
 * @brief Get the maximum X-dimension (in pixels)
 *
 * This is an implementation limit that may be lowered for systems with limited
 * memory by rebuilding the library.
 *
 * @see gs1_encoder_setPixMult()
 * @see gs1_encoder_getPixMult()
 *
 * @return maximum number pixel per module
 */
GS1_ENCODERS_API int gs1_encoder_getMaxPixMult(void);


/**
 * @brief Get the maximum height of GS1-128 linear symbols in modules
 *
 * This is an implementation limit that may be lowered for systems with limited
 * memory by rebuilding the library.
 *
 * @return maximum GS1-128 height in modules
 */
GS1_ENCODERS_API int gs1_encoder_getMaxUcc128LinHeight(void);


/**
 * @brief Initialise a new ::gs1_encoder context.
 *
 * If storage is provided then this will be used, however it is more typical to
 * pass NULL in which case the library will malloc its own storage.
 *
 * Any storage buffer provided must be sufficient for the instance.
 *
 * @see gs1_encoder_instanceSize()
 *
 * @param [in,out] mem buffer to use for storage, or NULL for automatic allocation
 * @return ::gs1_encoder context on success, else NULL.
 */
GS1_ENCODERS_API gs1_encoder* gs1_encoder_init(void *mem);


/**
 * @brief Read an error message generated by the library.
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return pointer to error message string
 */
GS1_ENCODERS_API char* gs1_encoder_getErrMsg(gs1_encoder *ctx);


/**
 * @brief Get the current symbology type.
 *
 * @see gs1_encoder_setSym()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return symbology type, one of ::symbologies
 */
GS1_ENCODERS_API int gs1_encoder_getSym(gs1_encoder *ctx);


/**
 * @brief Set the symbology type.
 *
 * This allows the symbology to be specified as any one of the known
 * ::symbologies other than ::gs1_encoder_sNONE or ::gs1_encoder_sNUMSYMS.
 *
 * @see gs1_encoder_getSym()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] sym symbology type
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setSym(gs1_encoder *ctx, int sym);


/**
 * @brief Get the current pixels per module ("X-dimension")
 *
 * @see gs1_encoder_setPixMult()
 * @see gs1_encoder_getMaxPixMult()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return pixels per module
 */
GS1_ENCODERS_API int gs1_encoder_getPixMult(gs1_encoder *ctx);


/**
 * @brief Set the pixels per module ("X-dimension").
 *
 * Valid options range from 1 up to the limit returned by gs1_encoder_getMaxPixMult().
 *
 * \note
 * The X and Y undercut will be reset if the new X dimension is insufficient.
 * The separator height will be updated to match the new X dimension as necessary.
 *
 * @see gs1_encoder_getPixMult()
 * @see gs1_encoder_getMaxPixMult()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] pixMult pixels per module
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setPixMult(gs1_encoder *ctx, int pixMult);


/**
 * @brief Get the current X undercut.
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current X undercut in pixels
 */
GS1_ENCODERS_API int gs1_encoder_getXundercut(gs1_encoder *ctx);


/**
 * @brief Set the X undercut.
 *
 * Compensate for horizontal print growth by shaving this number of pixels from
 * both sides of each module.
 *
 * \note
 * Must be less than half the X-dimension.
 *
 * @see gs1_encoder_getXundercut()
 * @see gs1_encoder_getPixMult()
 *
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] Xundercut in pixels
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setXundercut(gs1_encoder *ctx, int Xundercut);


/**
 * @brief Get the current Y undercut.
 *
 * @see gs1_encoder_setYundercut()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current Y undercut in pixels
 */
GS1_ENCODERS_API int gs1_encoder_getYundercut(gs1_encoder *ctx);


/**
 * @brief Set the Y undercut.
 *
 * Compensate for vertical print growth by shaving this number of pixels from
 * the top and bottom of each module.
 *
 * \note
 * Must be less than half the X-dimension.
 *
 * @see gs1_encoder_getYundercut()
 * @see gs1_encoder_getPixMult()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] Yundercut in pixels
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setYundercut(gs1_encoder *ctx, int Yundercut);


/**
 * @brief Get the current separator height between linear and 2D components.
 *
 * @see gs1_encoder_setSepHt()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current serparator height
 */
GS1_ENCODERS_API int gs1_encoder_getSepHt(gs1_encoder *ctx);


/**
 * @brief Set the separator height between linear and 2D components.
 *
 * \note
 * Valid values are 1 to 2 times the X dimension.
 *
 * @see gs1_encoder_getSepHt()
 * @see gs1_encoder_getPixMult()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] sepHt in pixels
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setSepHt(gs1_encoder *ctx, int sepHt);


/**
 * @brief Get the current number of segments per row for GS1 DataBar Expanded
 * Stacked symbols.
 *
 * @see gs1_encoder_setRssExpSegWidth()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current segments per row
 */
GS1_ENCODERS_API int gs1_encoder_getRssExpSegWidth(gs1_encoder *ctx);


/**
 * @brief Set the number of segments per row for GS1 DataBar Expanded Stacked
 * symbols.
 *
 * The default number of segments per row (22) matches the maximum number of
 * symbol character for GS1 DataBar Expanded, resulting in a non-stacked
 * symbol.
 *
 * @see gs1_encoder_getRssExpSegWidth()
 *
 * \note
 * Valid values are even numbers from 2 to 22.
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] rssExpSegWidth segments per row
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setRssExpSegWidth(gs1_encoder *ctx, int rssExpSegWidth);


/**
 * @brief Get the height of GS1-128 linear symbols in modules.
 *
 * @see gs1_encoder_setUcc128LinHeight
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current linear symbol height in modules
 */
GS1_ENCODERS_API int gs1_encoder_getUcc128LinHeight(gs1_encoder *ctx);


/**
 * @brief Set the height of GS1-128 linear symbols in modules.
 *
 * \note
 * Valid values are 1 to the value returned by gs1_encoder_getMaxUcc128LinHeight().
 *
 * @see gs1_encoder_getUcc128LinHeight()
 * @see gs1_encoder_getMaxUcc128LinHeight()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] ucc128linHeight height in modules
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setUcc128LinHeight(gs1_encoder *ctx, int ucc128linHeight);


/**
 * @brief Get the current fixed number of rows for Data Matrix symbols.
 *
 * @see gs1_encoder_setDmRows()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current number of fixed rows, or 0 if automatic
 */
GS1_ENCODERS_API int gs1_encoder_getDmRows(gs1_encoder *ctx);


/**
 * @brief Set a fixed number of rows for Data Matrix symbols.
 *
 * \note
 * Valid values are 8 to 144, or 0 for automatic
 *
 * @see gs1_encoder_getDmRows
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] rows number of fixed rows, or 0 for automatic
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setDmRows(gs1_encoder *ctx, int rows);


/**
 * @brief Get the current fixed number of columns for Data Matrix symbols.
 *
 * @see gs1_encoder_setDmColumns()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current number of fixed columns, or 0 if automatic
 */
GS1_ENCODERS_API int gs1_encoder_getDmColumns(gs1_encoder *ctx);


/**
 * @brief Set a fixed number of columns for Data Matrix symbols.
 *
 * @see gs1_encoder_getDmColumns()
 *
 * \note
 * Valid values are 10 to 144, or 0 for automatic
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] columns number of fixed columns, or 0 for automatic
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setDmColumns(gs1_encoder *ctx, int columns);


/**
 * @brief Get the current fixed version number for QR Code symbols.
 *
 * @see gs1_encoder_setQrVersion()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current version number, or 0 if automatic
 */
GS1_ENCODERS_API int gs1_encoder_getQrVersion(gs1_encoder *ctx);


/**
 * @brief Set a fixed version number for QR Code symbols.
 *
 * \note
 * Valid values are 1 to 40, or 0 for automatic
 *
 * @see gs1_encoder_getQrVersion()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] version fixed version number, or 0 for automatic
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setQrVersion(gs1_encoder *ctx, int version);


/**
 * @brief Get the current error correction level for QR Code symbols.
 *
 * @see gs1_encoder_setQrEClevel()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current error correction level, one of ::qrEClevel
 */
GS1_ENCODERS_API int gs1_encoder_getQrEClevel(gs1_encoder *ctx);


/**
 * @brief Set the error correction level for QR Code symbols.
 *
 * This determines what proportion of a symbols data can be reliably
 * reconstructed if it is damaged.
 *
 * Default is ::gs1_encoder_qrEClevelM
 *
 * @see ::qrEClevel
 * @see gs1_encoder_getQrEClevel()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] ecLevel error correction level, one of ::qrEClevel
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setQrEClevel(gs1_encoder *ctx, int ecLevel);


/**
 * @brief Get the current status of the "add check digit" mode.
 *
 * @see gs1_encoder_setAddCheckDigit()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return current status of the add check digit mode
 */
GS1_ENCODERS_API bool gs1_encoder_getAddCheckDigit(gs1_encoder *ctx);


/**
 * @brief Enable or disable "add check digit" mode for EAN/UPC and GS1 DataBar
 * symbols.
 *
 *   * If false (default), then the data string must contain a valid check digit.
 *   * If true, then the data string must not contain a check digit as one will
 *     be generated automatically.
 *
 * \note
 * This option is only valid for symbologies that accept fixed-length data,
 * specifically EAN/UPC and GS1 DataBar except Expanded (Stacked).
 *
 * @see gs1_encoder_getAddCheckDigit()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] addCheckDigit enabled if true; disabled if false
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setAddCheckDigit(gs1_encoder *ctx, bool addCheckDigit);


/**
 * @brief Indicates whether barcode data input is currently taken from a buffer
 * or a file.
 *
 * @see gs1_encoder_setFileInputFlag()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return true of input if from a file; false if from a buffer
 */
GS1_ENCODERS_API bool gs1_encoder_getFileInputFlag(gs1_encoder *ctx);


/**
 * @brief Selects between a file and buffer for barcode data input.
 * symbols.
 *
 *   * If false (default), then the content of the buffer set by
 *     gs1_encoder_setDataStr() is used from barcode data input.
 *   * If true, then the file specified by gs1_encoder_setDataFile() is used
 *     from barcode data input.
 *
 * \note
 * When a file is used for data input, any traling newline character is
 * stripped.
 *
 * @see gs1_encoder_getFileInputFlag)
 * @see gs1_encoder_setDataFile()
 * @see gs1_encoder_setDataStr()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] fileInputFlag file input if true; buffer input if false
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setFileInputFlag(gs1_encoder *ctx, bool fileInputFlag);


/**
 * @brief Reads the data from the buffer that is used when buffer input is
 * selected.
 *
 * @see gs1_encoder_getDataStr()
 * @see gs1_encoder_setFileInputFlag()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return a pointer to the data input buffer
 */
GS1_ENCODERS_API char* gs1_encoder_getDataStr(gs1_encoder *ctx);


/**
 * @brief Sets the raw data in the buffer that is used when buffer input is
 * selected.
 *
 * When the input begins with the "#" character the data is considered to be in
 * GS1 Application Identifier syntax. In this case, all instances of the "#"
 * character are considered for be FNC1 non-data characters that are used to
 * separate fields that are not specified as being pre-defined length from
 * subsequent fields.
 *
 * The acceptable raw data input varies between encoders.
 *
 * It is strongly advised that GS1 data input is specified using
 * gs1_encoder_setGS1dataStr() which takes care of the AI encoding rules
 * automatically, including insertion of FNC1 characters where required.
 *
 * \note
 * The length of the data must be less that the value returned by
 * gs1_encoder_getMaxInputBuffer().
 *
 * @see gs1_encoder_setGS1dataStr()
 * @see gs1_encoder_getMaxInputBuffer()
 * @see gs1_encoder_getDataStr()
 * @see gs1_encoder_setFileInputFlag()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] dataStr containing the raw barcode data
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setDataStr(gs1_encoder *ctx, char* dataStr);


/**
 * @brief Sets the data in the buffer that is used when buffer input is
 * selected by parsing input provided in GS1 Application Identifier syntax into
 * a raw data string.
 *
 * The input is provided in human-friendly format **without** FNC1 characters
 * which are inserted automatically, for example:
 *
 * \code
 * (01)12345678901231(10)ABC123(11)210630
 * \endcode
 *
 * This syntax harmonises the format for the input accepted by all symbologies.
 * For example the following input is acceptable for EAN-13, UPC-A, UPC-E and
 * any variant of the GS1 DataBar family:
 *
 * \code
 * (01)00031234000054
 * \endcode
 *
 * The input is immediately parsed and the resulting encoding is available via
 * gs1_encoder_getDataStr().
 *
 * For symbologies that support a composite component (all except
 * ::gs1_encoder_sDM and ::gs1_encoder_sQR), the data for the linear and 2D
 * components can be separated by a "|" character, for example:
 *
 * \code
 * (01)12345678901231|(10)ABC123(11)210630
 * \endcode
 *
 * \note
 * The ultimate length of the encoded data must be less that the value returned by
 * gs1_encoder_getMaxInputBuffer().
 *
 * @see gs1_encoder_getMaxInputBuffer()
 * @see gs1_encoder_getDataStr()
 * @see gs1_encoder_setFileInputFlag()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] dataStr the barcode input data in GS1 Application Identifier syntax
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setGS1dataStr(gs1_encoder *ctx, char* dataStr);


/**
 * @brief Gets the filename for a file containing the barcode data when file
 * input is selected.
 *
 * @see gs1_encoder_setDataFile()
 * @see gs1_encoder_setFileInputFlag()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return a pointer to a string containing the filename
 */
GS1_ENCODERS_API char* gs1_encoder_getDataFile(gs1_encoder *ctx);


/**
 * @brief Sets the filename for a file from which barcode data is read when file
 * input is selected.
 *
 * \note
 * The length of the filename must be less than the value returned by gs1_encoder_getMaxFilenameLength().
 *
 * @see gs1_encoder_getDataFile()
 * @see gs1_encoder_setFileInputFlag()
 * @see gs1_encoder_getMaxFilenameLength()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] dataFile the data input filename
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setDataFile(gs1_encoder *ctx, char* dataFile);


/**
 * @brief Get the current output format type.
 *
 * @see gs1_encoder_setFormat()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return output format, one of ::formats
 */
GS1_ENCODERS_API int gs1_encoder_getFormat(gs1_encoder *ctx);


/**
 * @brief Set the output format for the barcode symbol.
 *
 * This allows the output format to be specified as any one of the known
 * ::formats:
 *
 *   * ::gs1_encoder_dBMP: BMP format
 *   * ::gs1_encoder_dTIF: TIFF format
 *   * ::gs1_encoder_dRAW: TIFF format, without the header
 *
 * @see gs1_encoder_getFormat()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] format output format
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setFormat(gs1_encoder *ctx, int format);


/**
 * @brief Get the current output filename.
 *
 * @see gs1_encoder_setOutFile()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return output filename
 */
GS1_ENCODERS_API char* gs1_encoder_getOutFile(gs1_encoder *ctx);


/**
 * @brief Set the output file for the barcode symbol.
 *
 * The output will be written to the specified file in the format specified by
 * gs1_encoder_setFormat().
 *
 * If the filename is provided as the empty string then the output data will be
 * stored in the output buffer which can be read using gs1_encoder_getBuffer()
 * and gs1_encoder_getBufferStrings().
 *
 * \note
 * The length of the filename must be less than the value returned by
 * gs1_encoder_getMaxFilenameLength().
 *
 * @see gs1_encoder_getOutFile()
 * @see gs1_encoder_getBuffer()
 * @see gs1_encoder_getBufferStrings()
 * @see gs1_encoder_getMaxFilenameLength()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [in] outFile the output filename, or "" for buffer output
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_setOutFile(gs1_encoder *ctx, char* outFile);


/**
 * @brief Generate a barcode symbol representing the given input data
 *
 * This will create a barcode image for the symbology specified by
 * gs1_encoder_setSym() containing the data provided by gs1_encoder_setDataStr
 * or gs1_encoder_setGS1dataStr().
 *
 * If the input is valid for the selected symbology then the image output will
 * be written to the output file specified by gs1_encoder_setOutFile() or to
 * the output buffer if the output filename is empty in the format specifiede
 * by gs1_encoder_setFormat().
 *
 * @see gs1_encoder_setSym()
 * @see gs1_encoder_setDataStr()
 * @see gs1_encoder_setGS1dataStr()
 * @see gs1_encoder_setFormat()
 * @see gs1_encoder_setOutFile()
 *
 * @param ctx gs1_encoder context.
 * @return true on success, otherwise false and an error message is set
 */
GS1_ENCODERS_API bool gs1_encoder_encode(gs1_encoder *ctx);


/**
 * @brief Get the output buffer.
 *
 * @see gs1_encoder_setOutFile()
 *
 * \note
 * The storage for the string array is managed by the library and must not be
 * freed.
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [out] buffer a pointer to the buffer
 * @return length of the buffer
 */
GS1_ENCODERS_API size_t gs1_encoder_getBuffer(gs1_encoder *ctx, void** buffer);


/**
 * @brief Get the number of columns in the output buffer image.
 *
 * @see gs1_encoder_getBuffer()
 * @see gs1_encoder_getBufferStrings()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return the number of columns in the image held in the buffer
 */
GS1_ENCODERS_API int gs1_encoder_getBufferWidth(gs1_encoder *ctx);


/**
 * @brief Get the number of rows in the output buffer image.
 *
 * @see gs1_encoder_getBuffer()
 * @see gs1_encoder_getBufferStrings()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @return the number of rows in the image held in the buffer
 */
GS1_ENCODERS_API int gs1_encoder_getBufferHeight(gs1_encoder *ctx);


/**
 * @brief Return the output buffer represented as an array of strings
 *
 * Example output:
 *
 * \code
 * {
 *     "                             ",
 *     "                             ",
 *     "                             ",
 *     "                             ",
 *     "    XXXXXXX  X XX XXXXXXX    ",
 *     "    X     X  X  X X     X    ",
 *     "    X XXX X XXX X X XXX X    ",
 *     "    X XXX X X     X XXX X    ",
 *     "    X XXX X XXXXX X XXX X    ",
 *     "    X     X XX  X X     X    ",
 *     "    XXXXXXX X X X XXXXXXX    ",
 *     "            X XXX            ",
 *     "    X XXXXX  XX X XXXXX      ",
 *     "      XX X X X  X  X  X      ",
 *     "    XXXX XXX  XX X  X  X     ",
 *     "     XXX   X X     XXXXX     ",
 *     "      X   XX  XX X  X X      ",
 *     "            X XXXXXX  X      ",
 *     "    XXXXXXX   X X XX X X     ",
 *     "    X     X X  XXXX X X X    ",
 *     "    X XXX X X X X  X X X     ",
 *     "    X XXX X XXX X   X X      ",
 *     "    X XXX X XXXX X X XX      ",
 *     "    X     X  XX    XX X      ",
 *     "    XXXXXXX XX X X X X X     ",
 *     "                             ",
 *     "                             ",
 *     "                             ",
 *     "                             "
 * }
 * \endcode
 *
 * \note
 * The storage for the string array is managed by the library and must not be
 * freed.
 *
 * @see gs1_encoder_getBuffer()
 *
 * @param [in,out] ctx ::gs1_encoder context
 * @param [out] strings pointer to an array of strings
 * @return the number of rows in the returned array of strings
 */
GS1_ENCODERS_API size_t gs1_encoder_getBufferStrings(gs1_encoder *ctx, char*** strings);


/**
 *  @brief Destroy a ::gs1_encoder instance.
 *
 *  This will release all library-managed storage associated with the instance.
 *
 *  @param [in,out] ctx ::gs1_encoder context to destroy
 */
GS1_ENCODERS_API void gs1_encoder_free(gs1_encoder *ctx);


#ifdef __cplusplus
}
#endif


#endif /* GS1_ENCODERS_H */
