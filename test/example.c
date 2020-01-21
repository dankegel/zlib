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

typedef struct test_result_s {
    int           success; /* 1 when successful, 0 when failed with an
                           error code, -1 when failed without an error
                           code */
    int           err;     /* error code if success is not 1 */
    z_const char* message;
    z_const char* extended_message;
} test_result;

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        exit(1); \
    } \
}

#define CHECK_ERR2(err, msg, result) { \
    if (err != Z_OK) { \
        result.err = err; \
        result.success = 0; \
        result.message = msg; \
        return result; \
    } \
}

#define RETURN_ERR2(msg, result) { \
    result.success = -1; \
    result.message = msg; \
    return result; \
}

#define CHECK_ERR3(result) { \
    if (result.success == 0) { \
        fprintf(stderr, "%s error: %d\n", result.message, result.err); \
        exit(1); \
    } else if (result.success == -1) { \
        fprintf(stderr, result.message); \
        exit(1); \
    } else { \
        if (result.message != NULL) { \
            if (result.extended_message != NULL) { \
                printf(result.message, result.extended_message); \
            } else { \
                printf(result.message); \
            } \
        } \
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
    CHECK_ERR2(err, "compress", result);

    strcpy((char*)uncompr, "garbage");

    err = uncompress(uncompr, &uncomprLen, compr, comprLen);
    CHECK_ERR2(err, "uncompress", result);

    if (strcmp((char*)uncompr, hello)) {
        RETURN_ERR2("bad uncompress\n", result);
    } else {
        result.success = 1;
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
    result.success = -1;
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

    result.success = 1;
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
    CHECK_ERR2(err, "deflateInit", result);

    c_stream.next_in  = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = deflate(&c_stream, Z_NO_FLUSH);
        CHECK_ERR2(err, "deflate", result);
    }
    /* Finish the stream, still forcing small buffers: */
    for (;;) {
        c_stream.avail_out = 1;
        err = deflate(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR2(err, "deflate", result);
    }

    err = deflateEnd(&c_stream);
    CHECK_ERR2(err, "deflateEnd", result);

    result.success = 1;
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
    CHECK_ERR2(err, "inflateInit", result);

    while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        err = inflate(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR2(err, "inflate", result);
    }

    err = inflateEnd(&d_stream);
    CHECK_ERR2(err, "inflateEnd", result);

    if (strcmp((char*)uncompr, hello)) {
        RETURN_ERR2("bad inflate\n", result);
    } else {
        result.success = 1;
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
    CHECK_ERR2(err, "deflateInit", result);

    c_stream.next_out = compr;
    c_stream.avail_out = (uInt)comprLen;

    /* At this point, uncompr is still mostly zeroes, so it should compress
     * very well:
     */
    c_stream.next_in = uncompr;
    c_stream.avail_in = (uInt)uncomprLen;
    err = deflate(&c_stream, Z_NO_FLUSH);
    CHECK_ERR2(err, "deflate", result);
    if (c_stream.avail_in != 0) {
        RETURN_ERR2("deflate not greedy\n", result);
    }

    /* Feed in already compressed data and switch to no compression: */
    deflateParams(&c_stream, Z_NO_COMPRESSION, Z_DEFAULT_STRATEGY);
    c_stream.next_in = compr;
    c_stream.avail_in = (uInt)comprLen/2;
    err = deflate(&c_stream, Z_NO_FLUSH);
    CHECK_ERR2(err, "deflate", result);

    /* Switch back to compressing mode: */
    deflateParams(&c_stream, Z_BEST_COMPRESSION, Z_FILTERED);
    c_stream.next_in = uncompr;
    c_stream.avail_in = (uInt)uncomprLen;
    err = deflate(&c_stream, Z_NO_FLUSH);
    CHECK_ERR2(err, "deflate", result);

    err = deflate(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        RETURN_ERR2("deflate should report Z_STREAM_END\n", result);
    }
    err = deflateEnd(&c_stream);
    CHECK_ERR2(err, "deflateEnd", result);

    result.success = 1;
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
    CHECK_ERR2(err, "inflateInit", result);

    for (;;) {
        d_stream.next_out = uncompr;            /* discard the output */
        d_stream.avail_out = (uInt)uncomprLen;
        err = inflate(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR2(err, "large inflate", result);
    }

    err = inflateEnd(&d_stream);
    CHECK_ERR2(err, "inflateEnd", result);

    if (d_stream.total_out != 2*uncomprLen + comprLen/2) {
        // TODO(cblume): Handle this case
        fprintf(stderr, "bad large inflate: %ld\n", d_stream.total_out);
        exit(1);
    } else {
        result.success = 1;
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
    CHECK_ERR2(err, "deflateInit", result);

    c_stream.next_in  = (z_const unsigned char *)hello;
    c_stream.next_out = compr;
    c_stream.avail_in = 3;
    c_stream.avail_out = (uInt)*comprLen;
    err = deflate(&c_stream, Z_FULL_FLUSH);
    CHECK_ERR2(err, "deflate", result);

    compr[3]++; /* force an error in first compressed block */
    c_stream.avail_in = len - 3;

    err = deflate(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        CHECK_ERR2(err, "deflate", result);
    }
    err = deflateEnd(&c_stream);
    CHECK_ERR2(err, "deflateEnd", result);

    *comprLen = c_stream.total_out;

    result.success = 1;
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
    CHECK_ERR2(err, "inflateInit", result);

    d_stream.next_out = uncompr;
    d_stream.avail_out = (uInt)uncomprLen;

    err = inflate(&d_stream, Z_NO_FLUSH);
    CHECK_ERR2(err, "inflate", result);

    d_stream.avail_in = (uInt)comprLen-2;   /* read all compressed data */
    err = inflateSync(&d_stream);           /* but skip the damaged part */
    CHECK_ERR2(err, "inflateSync", result);

    err = inflate(&d_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        RETURN_ERR2("inflate should report Z_STREAM_END\n", result);
    }
    err = inflateEnd(&d_stream);
    CHECK_ERR2(err, "inflateEnd", result);

    result.success = 1;
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
    CHECK_ERR2(err, "deflateInit", result);

    err = deflateSetDictionary(&c_stream,
                (const Bytef*)dictionary, (int)sizeof(dictionary));
    CHECK_ERR2(err, "deflateSetDictionary", result);

    dictId = c_stream.adler;
    c_stream.next_out = compr;
    c_stream.avail_out = (uInt)comprLen;

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_in = (uInt)strlen(hello)+1;

    err = deflate(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        RETURN_ERR2("deflate should report Z_STREAM_END\n", result);
    }
    err = deflateEnd(&c_stream);
    CHECK_ERR2(err, "deflateEnd", result);

    result.success = 1;
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
    CHECK_ERR2(err, "inflateInit", result);

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
        CHECK_ERR(err, "inflate with dict");
    }

    err = inflateEnd(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (strcmp((char*)uncompr, hello)) {
        RETURN_ERR2("bad inflate with dict\n", result);
    } else {
        result.success = 1;
        result.message = "inflate with dictionary: %s\n";
        result.extended_message = (char*)uncompr;
        return result;
    }
}

/* ===========================================================================
 * Usage:  example [output.gz  [input.gz]]
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
    result = test_compress(compr, comprLen, uncompr, uncomprLen);
    CHECK_ERR3(result);

    result = test_gzio((argc > 1 ? argv[1] : TESTFILE),
                       uncompr, uncomprLen);
    CHECK_ERR3(result);
#endif

    result = test_deflate(compr, comprLen);
    CHECK_ERR3(result);
    result = test_inflate(compr, comprLen, uncompr, uncomprLen);
    CHECK_ERR3(result);

    result = test_large_deflate(compr, comprLen, uncompr, uncomprLen);
    CHECK_ERR3(result);
    result = test_large_inflate(compr, comprLen, uncompr, uncomprLen);
    CHECK_ERR3(result);

    result = test_flush(compr, &comprLen);
    CHECK_ERR3(result);
    result = test_sync(compr, comprLen, uncompr, uncomprLen);
    CHECK_ERR3(result);
    comprLen = uncomprLen;

    result = test_dict_deflate(compr, comprLen);
    CHECK_ERR3(result);
    result = test_dict_inflate(compr, comprLen, uncompr, uncomprLen);
    CHECK_ERR3(result);

    free(compr);
    free(uncompr);

    return 0;
}
