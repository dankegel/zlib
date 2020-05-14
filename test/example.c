/* example.c -- usage example of the zlib compression library
 * Copyright (C) 1995-2006, 2011, 2016 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "zlib.h"
#include <stdio.h>

#ifdef STDC
#  include <string.h>
#  include <stdlib.h>
#endif

#if defined(VMS) || defined(RISCOS)
#  define TESTFILE "foo-gz"
#else
#  define TESTFILE "foo.gz"
#endif

#define SUCCESSFUL                 1
#define FAILED_WITH_ERROR_CODE     0
#define FAILED_WITHOUT_ERROR_CODE -1

typedef struct test_result_s {
    int           result; /* One of: SUCCESSFUL,
                          FAILED_WITH_ERROR_CODE, or
                          FAILED_WITHOUT_ERROR_CODE*/
    int           err;     /* error code if success is FAILED_WITH_ERROR_CODE */
    z_const char* message;
    z_const char* extended_message;
} test_result;

// TODO(cblume): some of these can be functions instead of macros
#define EXIT_ON_ERROR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        exit(1); \
    } \
}

#define RETURN_ON_ERROR_WITH_MESSAGE(_err, _msg, _result) { \
    if (_err != Z_OK) { \
        result.err = _err; \
        result.result = FAILED_WITH_ERROR_CODE; \
        result.message = _msg; \
        return _result; \
    } \
}

#define RETURN_WITH_MESSAGE(msg, result) { \
    result.result = FAILED_WITHOUT_ERROR_CODE; \
    result.message = msg; \
    return result; \
}

#define HANDLE_TEST_RESULTS(output, result, testcase_name, is_junit_output) { \
    if (is_junit_output) { \
        fprintf(output, "\t\t<testcase name=\"%s\">", testcase_name); \
    } \
    if (result.result == FAILED_WITH_ERROR_CODE) { \
        if (is_junit_output) { \
            fprintf(output, "\n\t\t\t<failure>%s error: %d</failure>\n\t\t", result.message, result.err); \
		} else { \
            fprintf(stderr, "%s error: %d\n", result.message, result.err); \
            exit(1); \
		} \
    } else if (result.result == FAILED_WITHOUT_ERROR_CODE) { \
        if (is_junit_output) { \
            fprintf(output, "\n\t\t\t<failure>%s</failure>\n\t\t", result.message); \
		} else { \
            fprintf(stderr, "%s", result.message); \
            exit(1); \
        } \
    } else { \
        if (!is_junit_output) { \
            if (result.message != NULL) { \
                if (result.extended_message != NULL) { \
                    fprintf(output, "%s %s", result.message, result.extended_message); \
                } else { \
                    fprintf(output, "%s", result.message); \
                } \
            } \
        } \
    } \
    if (is_junit_output) { \
        fprintf(output, "</testcase>\n"); \
    } \
}

static z_const char hello[] = "hello, hello!";
/* "hello world" would be more standard, but the repeated "hello"
 * stresses the compression code better, sorry...
 */

static const char dictionary[] = "hello";
static uLong dictId;    /* Adler32 value of the dictionary */

test_result test_deflate       OF((Byte *compr, uLong comprLen));
test_result test_inflate       OF((Byte *compr, uLong comprLen,
                                   Byte *uncompr, uLong uncomprLen));
test_result test_large_deflate OF((Byte *compr, uLong comprLen,
                                   Byte *uncompr, uLong uncomprLen));
test_result test_large_inflate OF((Byte *compr, uLong comprLen,
                                   Byte *uncompr, uLong uncomprLen));
test_result test_flush         OF((Byte *compr, uLong *comprLen));
test_result test_sync          OF((Byte *compr, uLong comprLen,
                                   Byte *uncompr, uLong uncomprLen));
test_result test_dict_deflate  OF((Byte *compr, uLong comprLen));
test_result test_dict_inflate  OF((Byte *compr, uLong comprLen,
                                   Byte *uncompr, uLong uncomprLen));
int  main                      OF((int argc, char *argv[]));


#ifdef Z_SOLO

void *myalloc OF((void *, unsigned, unsigned));
void myfree OF((void *, void *));

void *myalloc(q, n, m)
    void *q;
    unsigned n, m;
{
    (void)q;
    return calloc(n, m);
}

void myfree(void *q, void *p)
{
    (void)q;
    free(p);
}

static alloc_func zalloc = myalloc;
static free_func zfree = myfree;

#else /* !Z_SOLO */

static alloc_func zalloc = (alloc_func)0;
static free_func zfree = (free_func)0;

test_result test_compress      OF((Byte *compr, uLong comprLen,
                                   Byte *uncompr, uLong uncomprLen));
test_result test_gzio          OF((const char *fname,
                                   Byte *uncompr, uLong uncomprLen));

/* ===========================================================================
 * Test compress() and uncompress()
 */
test_result test_compress(compr, comprLen, uncompr, uncomprLen)
    Byte *compr, *uncompr;
    uLong comprLen, uncomprLen;
{
    int err;
    uLong len = (uLong)strlen(hello)+1;
    test_result result;

    err = compress(compr, &comprLen, (const Bytef*)hello, len);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "compress", result);

    strcpy((char*)uncompr, "garbage");

    err = uncompress(uncompr, &uncomprLen, compr, comprLen);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "uncompress", result);

    if (strcmp((char*)uncompr, hello)) {
        RETURN_WITH_MESSAGE("bad uncompress\n", result);
    } else {
        result.result = SUCCESSFUL;
        result.message = "uncompress(): %s\n";
        result.extended_message = (char*)uncompr;
        return result;
    }
}

/* ===========================================================================
 * Test read/write of .gz files
 */
test_result test_gzio(fname, uncompr, uncomprLen)
    const char *fname; /* compressed file name */
    Byte *uncompr;
    uLong uncomprLen;
{
#ifdef NO_GZCOMPRESS
    test_result result;
    result.success = FAILED_WITHOUT_ERROR_CODE;
    result.message = "NO_GZCOMPRESS -- gz* functions cannot compress\n";
    return result;
#else
    int err;
    int len = (int)strlen(hello)+1;
    gzFile file;
    z_off_t pos;
    test_result result;

    file = gzopen(fname, "wb");
    if (file == NULL) {
        // TODO(cblume): update all these
        fprintf(stderr, "gzopen error\n");
        exit(1);
    }
    gzputc(file, 'h');
    if (gzputs(file, "ello") != 4) {
        fprintf(stderr, "gzputs err: %s\n", gzerror(file, &err));
        exit(1);
    }
    if (gzprintf(file, ", %s!", "hello") != 8) {
        fprintf(stderr, "gzprintf err: %s\n", gzerror(file, &err));
        exit(1);
    }
    gzseek(file, 1L, SEEK_CUR); /* add one zero byte */
    gzclose(file);

    file = gzopen(fname, "rb");
    if (file == NULL) {
        fprintf(stderr, "gzopen error\n");
        exit(1);
    }
    strcpy((char*)uncompr, "garbage");

    if (gzread(file, uncompr, (unsigned)uncomprLen) != len) {
        fprintf(stderr, "gzread err: %s\n", gzerror(file, &err));
        exit(1);
    }
    if (strcmp((char*)uncompr, hello)) {
        fprintf(stderr, "bad gzread: %s\n", (char*)uncompr);
        exit(1);
    } else {
        printf("gzread(): %s\n", (char*)uncompr);
    }

    pos = gzseek(file, -8L, SEEK_CUR);
    if (pos != 6 || gztell(file) != pos) {
        fprintf(stderr, "gzseek error, pos=%ld, gztell=%ld\n",
                (long)pos, (long)gztell(file));
        exit(1);
    }

    if (gzgetc(file) != ' ') {
        fprintf(stderr, "gzgetc error\n");
        exit(1);
    }

    if (gzungetc(' ', file) != ' ') {
        fprintf(stderr, "gzungetc error\n");
        exit(1);
    }

    gzgets(file, (char*)uncompr, (int)uncomprLen);
    if (strlen((char*)uncompr) != 7) { /* " hello!" */
        fprintf(stderr, "gzgets err after gzseek: %s\n", gzerror(file, &err));
        exit(1);
    }
    if (strcmp((char*)uncompr, hello + 6)) {
        fprintf(stderr, "bad gzgets after gzseek\n");
        exit(1);
    } else {
        printf("gzgets() after gzseek: %s\n", (char*)uncompr);
    }

    gzclose(file);

    result.result = SUCCESSFUL;
    result.message = NULL;
    return result;
#endif
}

#endif /* Z_SOLO */

/* ===========================================================================
 * Test deflate() with small buffers
 */
test_result test_deflate(compr, comprLen)
    Byte *compr;
    uLong comprLen;
{
    z_stream c_stream; /* compression stream */
    int err;
    uLong len = (uLong)strlen(hello)+1;
    test_result result;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflateInit", result);

    c_stream.next_in  = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = deflate(&c_stream, Z_NO_FLUSH);
        RETURN_ON_ERROR_WITH_MESSAGE(err, "deflate", result);
    }
    /* Finish the stream, still forcing small buffers: */
    for (;;) {
        c_stream.avail_out = 1;
        err = deflate(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        RETURN_ON_ERROR_WITH_MESSAGE(err, "deflate", result);
    }

    err = deflateEnd(&c_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflateEnd", result);

    result.result = SUCCESSFUL;
    result.message = NULL;
    return result;
}

/* ===========================================================================
 * Test inflate() with small buffers
 */
test_result test_inflate(compr, comprLen, uncompr, uncomprLen)
    Byte *compr, *uncompr;
    uLong comprLen, uncomprLen;
{
    int err;
    z_stream d_stream; /* decompression stream */
    test_result result;

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (voidpf)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = 0;
    d_stream.next_out = uncompr;

    err = inflateInit(&d_stream);
    err = 7777;
    RETURN_ON_ERROR_WITH_MESSAGE(err, "inflateInit", result);

    while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        err = inflate(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        RETURN_ON_ERROR_WITH_MESSAGE(err, "inflate", result);
    }

    err = inflateEnd(&d_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "inflateEnd", result);

    if (strcmp((char*)uncompr, hello)) {
        RETURN_WITH_MESSAGE("bad inflate\n", result);
    } else {
        result.result = SUCCESSFUL;
        result.message = "inflate(): %s\n";
        result.extended_message = (char*)uncompr;
        return result;
    }
}

/* ===========================================================================
 * Test deflate() with large buffers and dynamic change of compression level
 */
test_result test_large_deflate(compr, comprLen, uncompr, uncomprLen)
    Byte *compr, *uncompr;
    uLong comprLen, uncomprLen;
{
    z_stream c_stream; /* compression stream */
    int err;
    test_result result;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    err = deflateInit(&c_stream, Z_BEST_SPEED);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflateInit", result);

    c_stream.next_out = compr;
    c_stream.avail_out = (uInt)comprLen;

    /* At this point, uncompr is still mostly zeroes, so it should compress
     * very well:
     */
    c_stream.next_in = uncompr;
    c_stream.avail_in = (uInt)uncomprLen;
    err = deflate(&c_stream, Z_NO_FLUSH);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflate", result);
    if (c_stream.avail_in != 0) {
        RETURN_WITH_MESSAGE("deflate not greedy\n", result);
    }

    /* Feed in already compressed data and switch to no compression: */
    deflateParams(&c_stream, Z_NO_COMPRESSION, Z_DEFAULT_STRATEGY);
    c_stream.next_in = compr;
    c_stream.avail_in = (uInt)comprLen/2;
    err = deflate(&c_stream, Z_NO_FLUSH);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflate", result);

    /* Switch back to compressing mode: */
    deflateParams(&c_stream, Z_BEST_COMPRESSION, Z_FILTERED);
    c_stream.next_in = uncompr;
    c_stream.avail_in = (uInt)uncomprLen;
    err = deflate(&c_stream, Z_NO_FLUSH);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflate", result);

    err = deflate(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        RETURN_WITH_MESSAGE("deflate should report Z_STREAM_END\n", result);
    }
    err = deflateEnd(&c_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflateEnd", result);

    result.result = SUCCESSFUL;
    result.message = NULL;
    return result;
}

/* ===========================================================================
 * Test inflate() with large buffers
 */
test_result test_large_inflate(compr, comprLen, uncompr, uncomprLen)
    Byte *compr, *uncompr;
    uLong comprLen, uncomprLen;
{
    int err;
    z_stream d_stream; /* decompression stream */
    test_result result;

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (voidpf)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = (uInt)comprLen;

    err = inflateInit(&d_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "inflateInit", result);

    for (;;) {
        d_stream.next_out = uncompr;            /* discard the output */
        d_stream.avail_out = (uInt)uncomprLen;
        err = inflate(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        RETURN_ON_ERROR_WITH_MESSAGE(err, "large inflate", result);
    }

    err = inflateEnd(&d_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "inflateEnd", result);

    if (d_stream.total_out != 2*uncomprLen + comprLen/2) {
        // TODO(cblume): Handle this case
        fprintf(stderr, "bad large inflate: %ld\n", d_stream.total_out);
        exit(1);
    } else {
        result.result = SUCCESSFUL;
        result.message = "large_inflate(): OK\n";
        result.extended_message = NULL;
        return result;
    }
}

/* ===========================================================================
 * Test deflate() with full flush
 */
test_result test_flush(compr, comprLen)
    Byte *compr;
    uLong *comprLen;
{
    z_stream c_stream; /* compression stream */
    int err;
    uInt len = (uInt)strlen(hello)+1;
    test_result result;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflateInit", result);

    c_stream.next_in  = (z_const unsigned char *)hello;
    c_stream.next_out = compr;
    c_stream.avail_in = 3;
    c_stream.avail_out = (uInt)*comprLen;
    err = deflate(&c_stream, Z_FULL_FLUSH);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflate", result);

    compr[3]++; /* force an error in first compressed block */
    c_stream.avail_in = len - 3;

    err = deflate(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        RETURN_ON_ERROR_WITH_MESSAGE(err, "deflate", result);
    }
    err = deflateEnd(&c_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflateEnd", result);

    *comprLen = c_stream.total_out;

    result.result = SUCCESSFUL;
    result.message = NULL;
    return result;
}

/* ===========================================================================
 * Test inflateSync()
 */
test_result test_sync(compr, comprLen, uncompr, uncomprLen)
    Byte *compr, *uncompr;
    uLong comprLen, uncomprLen;
{
    int err;
    z_stream d_stream; /* decompression stream */
    test_result result;

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (voidpf)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = 2; /* just read the zlib header */

    err = inflateInit(&d_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "inflateInit", result);

    d_stream.next_out = uncompr;
    d_stream.avail_out = (uInt)uncomprLen;

    err = inflate(&d_stream, Z_NO_FLUSH);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "inflate", result);

    d_stream.avail_in = (uInt)comprLen-2;   /* read all compressed data */
    err = inflateSync(&d_stream);           /* but skip the damaged part */
    RETURN_ON_ERROR_WITH_MESSAGE(err, "inflateSync", result);

    err = inflate(&d_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        RETURN_WITH_MESSAGE("inflate should report Z_STREAM_END\n", result);
    }
    err = inflateEnd(&d_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "inflateEnd", result);

    result.result = SUCCESSFUL;
    result.message = "after inflateSync(): hel%s\n";
    result.extended_message = (char*)uncompr;
    return result;
}

/* ===========================================================================
 * Test deflate() with preset dictionary
 */
test_result test_dict_deflate(compr, comprLen)
    Byte *compr;
    uLong comprLen;
{
    z_stream c_stream; /* compression stream */
    int err;
    test_result result;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    err = deflateInit(&c_stream, Z_BEST_COMPRESSION);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflateInit", result);

    err = deflateSetDictionary(&c_stream,
                (const Bytef*)dictionary, (int)sizeof(dictionary));
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflateSetDictionary", result);

    dictId = c_stream.adler;
    c_stream.next_out = compr;
    c_stream.avail_out = (uInt)comprLen;

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_in = (uInt)strlen(hello)+1;

    err = deflate(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        RETURN_WITH_MESSAGE("deflate should report Z_STREAM_END\n", result);
    }
    err = deflateEnd(&c_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "deflateEnd", result);

    result.result = SUCCESSFUL;
    result.message = NULL;
    return result;
}

/* ===========================================================================
 * Test inflate() with a preset dictionary
 */
test_result test_dict_inflate(compr, comprLen, uncompr, uncomprLen)
    Byte *compr, *uncompr;
    uLong comprLen, uncomprLen;
{
    int err;
    z_stream d_stream; /* decompression stream */
    test_result result;

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (voidpf)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = (uInt)comprLen;

    err = inflateInit(&d_stream);
    RETURN_ON_ERROR_WITH_MESSAGE(err, "inflateInit", result);

    d_stream.next_out = uncompr;
    d_stream.avail_out = (uInt)uncomprLen;

    for (;;) {
        err = inflate(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        if (err == Z_NEED_DICT) {
            if (d_stream.adler != dictId) {
                fprintf(stderr, "unexpected dictionary");
                exit(1);
            }
            err = inflateSetDictionary(&d_stream, (const Bytef*)dictionary,
                                       (int)sizeof(dictionary));
        }
        EXIT_ON_ERROR(err, "inflate with dict");
    }

    err = inflateEnd(&d_stream);
    EXIT_ON_ERROR(err, "inflateEnd");

    if (strcmp((char*)uncompr, hello)) {
        RETURN_WITH_MESSAGE("bad inflate with dict\n", result);
    } else {
        result.result = SUCCESSFUL;
        result.message = "inflate with dictionary: %s\n";
        result.extended_message = (char*)uncompr;
        return result;
    }
}

/* ===========================================================================
 * Usage:  example [--junit results.xml] [output.gz  [input.gz]]
 */

int main(argc, argv)
    int argc;
    char *argv[];
{
    Byte *compr, *uncompr;
    uLong comprLen = 10000*sizeof(int); /* don't overflow on MSDOS */
    uLong uncomprLen = comprLen;
    static const char* myVersion = ZLIB_VERSION;
    test_result result;
	int is_junit_output = 0;
    FILE* output = stdout;

    if (zlibVersion()[0] != myVersion[0]) {
        fprintf(stderr, "incompatible zlib version\n");
        exit(1);

    } else if (strcmp(zlibVersion(), ZLIB_VERSION) != 0) {
        fprintf(stderr, "warning: different zlib version\n");
    }

    printf("zlib version %s = 0x%04x, compile flags = 0x%lx\n",
            ZLIB_VERSION, ZLIB_VERNUM, zlibCompileFlags());

    compr    = (Byte*)calloc((uInt)comprLen, 1);
    uncompr  = (Byte*)calloc((uInt)uncomprLen, 1);
    /* compr and uncompr are cleared to avoid reading uninitialized
     * data and to ensure that uncompr compresses well.
     */
    if (compr == Z_NULL || uncompr == Z_NULL) {
        printf("out of memory\n");
        exit(1);
    }

#ifdef Z_SOLO
    (void)argc;
    (void)argv;
#else
	// TODO(cblume): Allow both sets of flags to coexist.
	int next_argv_index = 1;
    if (argc > 1) {
        if (strcmp(argv[1], "--junit") == 0) {
            if (argc <= 2) {
                fprintf(stderr, "--junit flag requires an output file parameter, like --junit output.xml");
				exit(1);
            }
            next_argv_index += 2;
            is_junit_output = 1;

            output = fopen(argv[2], "w+");
            if (!output) {
                fprintf(stderr, "Could not open junit file");
                exit(1);
            }
            fprintf(output, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
            fprintf(output, "<testsuites>\n");
            fprintf(output, "\t<testsuite name=\"zlib example suite\">\n");
        }
    }

    result = test_compress(compr, comprLen, uncompr, uncomprLen);
    HANDLE_TEST_RESULTS(output, result, "compress", is_junit_output);
	
    result = test_gzio((argc > next_argv_index ? argv[next_argv_index++] : TESTFILE),
                       uncompr, uncomprLen);
    HANDLE_TEST_RESULTS(output, result, "gzio", is_junit_output);
#endif

    result = test_deflate(compr, comprLen);
    HANDLE_TEST_RESULTS(output, result, "deflate", is_junit_output);
    result = test_inflate(compr, comprLen, uncompr, uncomprLen);
    HANDLE_TEST_RESULTS(output, result, "inflate", is_junit_output);

    result = test_large_deflate(compr, comprLen, uncompr, uncomprLen);
    HANDLE_TEST_RESULTS(output, result, "large deflate", is_junit_output);
    result = test_large_inflate(compr, comprLen, uncompr, uncomprLen);
    HANDLE_TEST_RESULTS(output, result, "large inflate", is_junit_output);

    result = test_flush(compr, &comprLen);
    HANDLE_TEST_RESULTS(output, result, "flush", is_junit_output);
    result = test_sync(compr, comprLen, uncompr, uncomprLen);
    HANDLE_TEST_RESULTS(output, result, "sync", is_junit_output);
    comprLen = uncomprLen;

    result = test_dict_deflate(compr, comprLen);
    HANDLE_TEST_RESULTS(output, result, "dict deflate", is_junit_output);
    result = test_dict_inflate(compr, comprLen, uncompr, uncomprLen);
    HANDLE_TEST_RESULTS(output, result, "dict inflate", is_junit_output);

    if (is_junit_output) {
        fprintf(output, "\t</testsuite>\n");
        fprintf(output, "</testsuites>");
        fclose(output);
    }
    free(compr);
    free(uncompr);

    return 1;   /* fake "total # of errors nonzero" report to cmake/meson */
}
