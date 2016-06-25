#if !defined(__ASN1_H__)
#define __ASN1_H__

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <values.h>
#include <exception>
#include <string>
#include <sstream>
#include <cassert>
#include <iostream>
#include <fstream>
#include "dstream.h"
#include "user_exception.h"

namespace ai
{
namespace sg
{

using namespace std;

typedef int AsnLen;
typedef long AsnInt;
typedef unsigned long UAsnInt;
typedef int AsnTag;
typedef double AsnReal;

enum BER_CLASS
{
    ANY_CLASS = -2,
    NULL_CLASS = -1,
    UNIV = 0,
    APPL = (1 << 6),
    CNTX = (2 << 6),
    PRIV = (3 << 6)
};

enum BER_FORM
{
    ANY_FORM = -2,
    NULL_FORM = -1,
    PRIM = 0,
    CONS = (1 << 5)
};

enum BER_UNIV_CODE
{
    NO_TAG_CODE = 0,
    BOOLEAN_TAG_CODE = 1,
    INTEGER_TAG_CODE,
    BITSTRING_TAG_CODE,
    OCTETSTRING_TAG_CODE,
    NULLTYPE_TAG_CODE,
    OID_TAG_CODE,
    OD_TAG_CODE,
    EXTERNAL_TAG_CODE,
    REAL_TAG_CODE,
    ENUM_TAG_CODE,
    SEQ_TAG_CODE =  16,
    SET_TAG_CODE,
    NUMERICSTRING_TAG_CODE,
    PRINTABLESTRING_TAG_CODE,
    TELETEXSTRING_TAG_CODE,
    VIDEOTEXSTRING_TAG_CODE,
    IA5STRING_TAG_CODE,
    UTCTIME_TAG_CODE,
    GENERALIZEDTIME_TAG_CODE,
    GRAPHICSTRING_TAG_CODE,
    VISIBLESTRING_TAG_CODE,
    GENERALSTRING_TAG_CODE
};

const unsigned int ENC_PLUS_INFINITY = 0x40;
const unsigned int ENC_MINUS_INFINITY = 0x41;
const unsigned int REAL_BINARY = 0x80;
const unsigned int REAL_SIGN = 0x40;
const unsigned int REAL_EXPLEN_MASK = 0x03;
const unsigned int REAL_EXPLEN_1 = 0x00;
const unsigned int REAL_EXPLEN_2 = 0x01;
const unsigned int REAL_EXPLEN_3 = 0x02;
const unsigned int REAL_EXPLEN_LONG = 0x03;
const unsigned int REAL_FACTOR_MASK = 0x0c;
const unsigned int REAL_BASE_MASK = 0x30;
const unsigned int REAL_BASE_2 = 0x00;
const unsigned int REAL_BASE_8 = 0x10;
const unsigned int REAL_BASE_16 = 0x20;
const AsnReal PLUS_INFINITY = MAXDOUBLE;
const AsnReal MINUS_INFINITY = -MAXDOUBLE;
const int TB = sizeof(AsnTag);

#define MAKE_TAG_ID_CODE1(cd) (cd << ((TB -1) * 8))
#define MAKE_TAG_ID_CODE2(cd) ((31 << ((TB -1) * 8)) | (cd << ((TB-2) * 8)))
#define MAKE_TAG_ID_CODE3(cd) \
    ((31 << ((TB -1) * 8)) \
    | ((cd & 0x3f80) << 9) \
    | ( 0x0080 << ((TB-2) * 8)) \
    | ((cd & 0x007F) << ((TB-3)* 8)))
#define MAKE_TAG_ID_CODE4(cd) \
    ((31 << ((TB -1) * 8)) \
    | ((cd & 0x1fc000) << 2) \
    | ( 0x0080 << ((TB-2) * 8)) \
    | ((cd & 0x3f80) << 1) \
    | ( 0x0080 << ((TB-3) * 8)) \
    | ((cd & 0x007F) << ((TB-4)*8)))
#define MAKE_TAG_ID_CODE(cd) \
    ((cd < 31) ? (MAKE_TAG_ID_CODE1(cd)): \
    ((cd < 128) ? (MAKE_TAG_ID_CODE2(cd)): \
    ((cd < 16384) ? (MAKE_TAG_ID_CODE3(cd)): \
    MAKE_TAG_ID_CODE4(cd))))
#define MAKE_TAG_ID(cl, fm, cd) \
    ((static_cast<AsnTag>(cl) << ((TB - 1) * 8)) \
    | (static_cast<AsnTag>(fm) << ((TB - 1) * 8)) \
    | MAKE_TAG_ID_CODE(static_cast<AsnTag>(cd)))

// Max buffer length.
int const MAX_ASN1_LEN = 8192;

// Exception thrown when asn1 error.
class bad_asn1 : public exception
{
public:
    bad_asn1(const string& file, int line, int sys_code_, const string& cause) throw();
    ~bad_asn1() throw();
    const char* what () const throw();

private:
    int sys_code;
    string what_;
};

// Decode binary bytes in asn.1 format.
class asn1
{
public:
    asn1(const char* data_, int len, size_t buf_len = MAX_ASN1_LEN);
    asn1(std::istream* fs, size_t buf_len = MAX_ASN1_LEN);
    ~asn1();

    // Set decode buffer.
    void set_buf(const char* data_, int len);
    // Get byte of current buffer.
    unsigned char get_byte();
    // Skip some bytes.
    void buf_skip(int len_);
    void seek_back(int len_);
    void loc_back(int len_);
    size_t tell();
    // Get decode length.
    AsnLen dec_len(AsnLen& bytes, bool var = false);
    // Get decode tag.
    AsnTag dec_tag(AsnLen& tag, AsnLen block_filler = -1);
    // Decode int.
    void dec_int(AsnInt& result, AsnLen& bytes, AsnTag tag = MAKE_TAG_ID(UNIV, PRIM, INTEGER_TAG_CODE));
    // Decode unsigned int.
    void dec_uint(UAsnInt& result, AsnLen& bytes, AsnTag tag = MAKE_TAG_ID(UNIV, PRIM, INTEGER_TAG_CODE));
    // Decode float/double.
    void dec_real(AsnReal& result, AsnLen& bytes, AsnTag tag = MAKE_TAG_ID(UNIV, PRIM, REAL_TAG_CODE));
    // Decode string.
    void dec_string(char* result, AsnLen& bytes, AsnTag tag = MAKE_TAG_ID(UNIV, PRIM, OCTETSTRING_TAG_CODE));

    // Convert tbcd to ascii.
    static int tbcd2asc(char* bcd, char* str, int len);
    // Convert bcd to ascii.
    static int bcd2asc(char* bcd, char* str, int len);
    // Convert bcd to ascii.
    static int bcd2asc(unsigned char* bcd, char* str, int len);
	// trim alpha
	static void trim_alpha(char* number);

    // Convert tbcd to ascii.
    static int realtbcd2asc(char* bcd, char* str, int len);
    // Convert bcd to ascii.
    static int realbcd2asc(char* bcd, char* str, int len);
    // Convert bcd to ascii.
    static int realbcd2asc(unsigned char* bcd, char* str, int len);


    // Decode int content.
    void dec_int_content(AsnTag tag, AsnLen Len, AsnInt& result, AsnLen& bytes);
    // Decode unsigned int content.
    void dec_uint_content(AsnTag tag, AsnLen len, UAsnInt& result, AsnLen& bytes);
    // Decode float/double content.
    void dec_real_content(AsnTag tag, AsnLen len, AsnReal& result, AsnLen& bytes);
    // Decode string content.
    void dec_string_content(AsnTag tag, AsnLen len, char* result, AsnLen& bytes);
    // Decode octet string content.
    void dec_octet_string_content(AsnLen len, char* result, int max_len, AsnLen& bytes);
    // Whether has reached to the end.
    bool eof() const;
    // Copy some bytes to dst.
    void buf_copy(char* dst,int len);


private:
    // Initialize decode buffer.
    void buf_init(int len);

    void file_init(std::istream* fs);

    size_t fread(const size_t len);

    char* data;           //internal buffer to store bytes in memory.
    size_t block_len;

    char* data_begin;      //point the begining position in data
    char* data_end;        //point the ending position in data
    char* read_loc;        //point the current position in data

    std::istream* ifs;            //if file mode, store the income FILE pointer.
    size_t offset_end;
    size_t offset_begin;
    size_t offset_loc;

};

}
}

#endif


