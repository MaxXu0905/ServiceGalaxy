#include <errno.h>
#include "asn1.h"

namespace ai
{
namespace sg
{

using namespace std;

// ------------------------------------------ Exception throw by class asn1 ---------------------------------
bad_asn1::bad_asn1(const string& file, int line, int sys_code_, const string& cause) throw()
{
    what_ = (_("ERROR: In file {1}:{2}: cause: {3}") % file % line % cause).str(SGLOCALE);
}

bad_asn1::~bad_asn1() throw()
{
}

const char* bad_asn1::what () const throw()
{
    return what_.c_str();
}

// ------------------------------------------------ ASN1 ----------------------------------------------------
asn1::asn1(const char* data_, int len, size_t buf_len)
{
    data = new char[buf_len];
    block_len = buf_len;
    assert(data);
    set_buf(data_, len);
    file_init(NULL);
}

asn1::asn1(istream* fs, size_t buf_len)
{
    data = new char[buf_len];
    block_len = buf_len;
    assert(data);
    assert(fs);
    file_init(fs);
}

asn1::~asn1()
{
    if (data != NULL) {
        delete[] data;
    }
}

void asn1::file_init(istream* fs)
{
    ifs = fs;
    if (ifs != NULL) {
        offset_begin = ifs->tellg();
        offset_loc = ifs->tellg();
        ifs->seekg(0, std::ios::end);
        offset_end = ifs->tellg();
        ifs->seekg(0, std::ios_base::beg);
        buf_init(0);

        fread(block_len);
    } else {
        offset_begin = offset_end = offset_loc = 0;
    }
}

size_t asn1::fread(const size_t len)
{
    memmove(data,read_loc,data_end - read_loc);
    size_t read_len = std::min(len,block_len - (data_end - read_loc));

    ifs->read(data + (data_end - read_loc),read_len);
    size_t ret = ifs->gcount();

    if (!ifs) {
        throw bad_file(__FILE__,__LINE__, 17, bad_file::bad_read,strerror(errno));
    }
    offset_loc += ret;
    data_begin = data;
    data_end = data_begin + (data_end - read_loc + ret);
    read_loc = data_begin;
    return ret;
}
void asn1::set_buf(const char* data_, int len)
{
    assert(len > 0 && len <= block_len);
    len = len;
    memcpy(data, data_, len);
    buf_init(len);
}

void asn1::buf_init(int len)
{
    data_begin = data;
    data_end = data + len;
    read_loc = data_begin;
}

bool asn1::eof() const
{
    if (ifs == NULL)
        return (read_loc >= data_end);
    else
        return (read_loc >= data_end && offset_loc == offset_end);

}

void asn1::buf_copy(char* dst,int len)
{
    if (read_loc + len > data_end)
    {
        if (ifs == NULL) {
            throw bad_asn1(__FILE__, __LINE__, 18, (_("ERROR: file not open.")).str(SGLOCALE));
        }else{
            size_t read_len = 0;
            //memcpy(dst,read_loc,data_end - read_loc);
            //read_len += data_end - read_loc;
            while(read_len < len){
                fread(block_len);
                memcpy(dst + read_len,read_loc,std::min(block_len,len - read_len));

                read_loc += std::min(block_len,len-read_len);
                read_len += std::min(block_len,len-read_len);

                if (eof() && read_len < len) {
                    throw bad_asn1(__FILE__,__LINE__, 19, (_("ERROR: file end reached.")).str(SGLOCALE));
                }
            }
        }
    }
    else
    {
        memcpy(dst, read_loc, len);
        read_loc += len;
    }
}

unsigned char asn1::get_byte()
{
    if (eof())
        throw bad_asn1(__FILE__, __LINE__, 20, (_("ERROR: file end reached.")).str(SGLOCALE));

    if (ifs != NULL && read_loc >= data_end) {
        fread(block_len);
    }

    return static_cast<unsigned char>(*read_loc++);
}

AsnTag asn1::dec_tag (AsnLen& bytes, AsnLen block_filler)
{
    AsnTag tag_id = get_byte();
    AsnTag ret_tag;
    bytes++;
    if ((tag_id & 0x1f) == 0x1f) // Tag is more than 2 bytes, else is of 1 byte.
    {
        if (tag_id == block_filler)
            return tag_id;

        while ((ret_tag = get_byte()) & 0x80)
        {
            tag_id <<= 8;
            tag_id |= ret_tag;
            bytes++;

        }

        tag_id <<= 8;
        tag_id |= ret_tag;
        bytes++;
/*
        if (!eof())
        {
            tag_id |= get_byte();
            bytes++;
        }
        else
        {
            tag_id |= 0xFF;
        }
*/
    }
    return tag_id;
}

AsnLen asn1::dec_len(AsnLen& bytes, bool var)
{
    AsnLen byte = get_byte();
    bytes++;
    if (byte < 128) // The most bit is not 1.
    {
        return byte;
    }
    else if (byte == 0x080) // Indefinite length.
    {
        if (var)
            return -1;
        else
            throw bad_asn1(__FILE__, __LINE__, 21, (_("ERROR: indefinite length")).str(SGLOCALE));
    }
    else // Length is of more bytes.
    {
        int lenbytes = byte & 0x7f;
        if (lenbytes > sizeof(AsnLen))
            throw bad_asn1(__FILE__, __LINE__, 22, (_("ERROR: length overflow")).str(SGLOCALE));
        bytes += lenbytes;
        AsnLen len;
        for (len = 0; lenbytes > 0; lenbytes--)
            len = (len << 8) | get_byte();
        return len;
    }
}

void asn1::dec_int(AsnInt& result, AsnLen& bytes, AsnTag tag_id)
{
    AsnTag tag = dec_tag(bytes);
    if (tag != tag_id)
        throw bad_asn1(__FILE__, __LINE__, 23, (_("ERROR: wrong tag of integer")).str(SGLOCALE));
    AsnLen element_len = dec_len(bytes);
    dec_int_content(tag, element_len, result, bytes);
}

void asn1::dec_int_content(AsnTag tag_id, AsnLen len, AsnInt& result, AsnLen& bytes)
{
    if (len > sizeof(AsnInt))
        throw bad_asn1(__FILE__, __LINE__, 24, (_("ERROR: integer too big")).str(SGLOCALE));
    UAsnInt byte = get_byte();
    if (byte & 0x80)
        result = (-1 << 8) | byte;
    else
        result = byte;
    for (int i = 1; i < len; i++)
        result = (result << 8) | get_byte();
    bytes += len;
}

void asn1::dec_uint(UAsnInt& result, AsnLen& bytes, AsnTag tag_id)
{
    AsnTag tag = dec_tag(bytes);
    if (tag != tag_id)
        throw bad_asn1(__FILE__, __LINE__, 23, (_("ERROR: wrong tag of integer")).str(SGLOCALE));
    AsnLen element_len = dec_len(bytes);
    dec_uint_content(tag, element_len, result, bytes);
}

void asn1::dec_uint_content (AsnTag tag, AsnLen len, UAsnInt& result, AsnLen& bytes)
{
    if (len > sizeof(UAsnInt))
        throw bad_asn1(__FILE__, __LINE__, 24, (_("ERROR: unsigned integer too big")).str(SGLOCALE));
    UAsnInt byte = get_byte();
    result = byte;
    for (int i = 1; i < len; i++)
        result = (result << 8) | get_byte();
    bytes += len;
}

void asn1::dec_real(AsnReal& result, AsnLen& bytes, AsnTag tag_id)
{
    AsnTag tag = dec_tag(bytes);
    if (tag != tag_id)
        throw bad_asn1(__FILE__, __LINE__, 23, (_("ERROR: wrong tag of real")).str(SGLOCALE));
    AsnLen element_len = dec_len(bytes);
    dec_real_content(tag, element_len, result, bytes);
}

void asn1::dec_real_content(AsnTag tag_id, AsnLen len, AsnReal& result, AsnLen& bytes)
{
    unsigned char first_octet;
    unsigned char first_exp;
    int i;
    unsigned int exp_len;
    double mantissa;
    double base;
    long exponent;

    if (len == 0)
    {
        result = 0.0;
    }
    else
    {
        first_octet = get_byte();
        if (len == 1) // Only one byte.
        {
            bytes++;
            if (first_octet == ENC_PLUS_INFINITY)
                result = PLUS_INFINITY;
            else if (first_octet == ENC_MINUS_INFINITY)
                result = MINUS_INFINITY;
            else
                throw bad_asn1(__FILE__, __LINE__, 25, (_("ERROR: unrecognized real number of length 1 octet")).str(SGLOCALE));
        }
        else // More bytes.
        {
            if (first_octet & REAL_BINARY)
            {
                first_exp = get_byte();
                if (first_exp & 0x80)
                    exponent = -1;
                switch (first_octet & REAL_EXPLEN_MASK)
                {
                case REAL_EXPLEN_1:
                    exp_len = 1;
                    exponent =  (exponent << 8) | static_cast<long>(first_exp);
                    break;
                case REAL_EXPLEN_2:
                    exp_len = 2;
                    exponent = (exponent << 16)
                        | (static_cast<long>(first_exp) << 8)
                        | static_cast<long>(get_byte());
                    break;
                case REAL_EXPLEN_3:
                    exp_len = 3;
                    exponent = (exponent << 16)
                        | (static_cast<long>(first_exp) << 8)
                        | static_cast<long>(get_byte());
                    exponent = (exponent << 8) | static_cast<long>(get_byte());
                    break;
                default: // long form
                    exp_len = first_exp + 1;
                    i = first_exp - 1;
                    first_exp = get_byte();
                    if (first_exp & 0x80)
                        exponent = (-1L << 8) | static_cast<long>(first_exp);
                    else
                        exponent = static_cast<long>(first_exp);
                    for (; i > 0; first_exp--)
                        exponent = (exponent << 8) | static_cast<long>(get_byte());
                    break;
                }

                mantissa = 0.0;
                for (i = 1 + exp_len; i < len; i++)
                {
                    mantissa *= (1 << 8);
                    mantissa += get_byte();
                }
                mantissa *= (1 << ((first_octet & REAL_FACTOR_MASK) >> 2));

                switch (first_octet & REAL_BASE_MASK)
                {
                case REAL_BASE_2:
                    base = 2.0;
                    break;
                case REAL_BASE_8:
                    base = 8.0;
                    break;
                case REAL_BASE_16:
                    base = 16.0;
                    break;
                default:
                    throw bad_asn1(__FILE__, __LINE__, 26, (_("ERROR: unsupported base for a binary real number")).str(SGLOCALE));
                }

                result =  mantissa * pow(base, static_cast<double>(exponent));
                if (first_octet & REAL_SIGN)
                    result = -result;
                bytes += len;
            }
            else
            {
                throw bad_asn1(__FILE__, __LINE__, 27, (_("ERROR: decimal REAL form is not supported")).str(SGLOCALE));
            }
        }
    }
}

void asn1::dec_string(char* result, AsnLen& bytes, AsnTag tag_id)
{
    AsnTag tag = dec_tag(bytes);
    if (tag != tag_id)
        throw bad_asn1(__FILE__, __LINE__, 23, (_("ERROR: wrong tag of real")).str(SGLOCALE));
    AsnLen element_len = dec_len(bytes);
    dec_string_content(tag, element_len, result, bytes);
}

void asn1::dec_string_content(AsnTag tag_id, AsnLen len, char* result, AsnLen& bytes)
{
    buf_copy(result, len);
    result[len] = '\0';
    bytes += len;
}

void asn1::dec_octet_string_content(AsnLen len, char* result, int max_len, AsnLen& bytes)
{
    char buf[1024];
    int ret_len = 0;

    buf_copy(buf, len);
    for (int i = 0; i < len; i++) {
        ret_len += sprintf(result + ret_len, "%d", static_cast<int>(buf[i]));
        if (ret_len > max_len)
            throw bad_asn1(__FILE__, __LINE__, 22, (_("ERROR: length overflow")).str(SGLOCALE));
    }
    bytes += len;
}

void asn1::buf_skip(int len)
{
    if (eof())
        throw bad_asn1(__FILE__, __LINE__, 20, (_("ERROR: file end reached.")).str(SGLOCALE));

    if (ifs == NULL) {
        read_loc += len;
        if (read_loc > data_end)
            throw bad_asn1(__FILE__, __LINE__, 20, (_("ERROR: file end reached.")).str(SGLOCALE));
    }else{
        if(read_loc + len <= data_end)
            read_loc += len;
        else{
            size_t skip_len = 0;
            skip_len += data_end - read_loc;
            read_loc = data_end;
            while(skip_len < len){
                size_t ret = fread(block_len);
                read_loc += std::min(ret, len - skip_len);
                skip_len += std::min(ret, len - skip_len);
                if (eof() && skip_len < len) {
                    throw bad_asn1(__FILE__,__LINE__,20,(_("ERROR: file end reached.")).str(SGLOCALE));
                }
            }
        }
    }

}
size_t asn1::tell()
{
    if (ifs == NULL) {
        return read_loc - data_begin;
    }else{
        return offset_loc - (data_end - read_loc);
    }
}

void asn1::seek_back(int len_)
{
    if (data_begin + len_ <= read_loc) {
        read_loc -= len_;
    } else if (ifs == NULL) {
        throw bad_asn1(__FILE__,__LINE__,18,(_("ERROR: file not open.")).str(SGLOCALE));
    } else {
        size_t seek_len = len_ + (data_end - read_loc);

        if(offset_loc - seek_len < offset_begin)
            throw bad_asn1(__FILE__,__LINE__,28,(_("ERROR, file limit.")).str(SGLOCALE));

        ifs->seekg(offset_loc - seek_len,std::ios_base::beg);
        if(!ifs)
            throw bad_file(__FILE__,__LINE__,28,bad_file::bad_seek,(_("ERROR: Can't open file, {1}.") % strerror(errno)).str(SGLOCALE));
        buf_init(0);
        fread(block_len);
    }
}

void asn1::loc_back(int len_)
{
    read_loc -= len_;
}

int asn1::tbcd2asc(char* bcd, char* str, int len)
{
#if defined(BCD_AB)
    const char hex_string[] = "0123456789ABCDEF";
#else
    const char hex_string[] = "0123456789*#CDEF";
#endif
    int digit;
    int i = 0;
    int j = 0;

    while (1)
    {
        // least 4 bits.
        digit = bcd[i] & 0x0F;
        if (digit < 0x0F)
            str[j++] = hex_string[digit];
        else
            break;
        if (j >= len)
            break;
        // Most 4 bits.
        digit = (bcd[i++] >> 4) & 0x0F;
        if (digit < 0x0F)
            str[j++] = hex_string[digit];
        else
            break;
        if (j >= len)
            break;
    }
    str[j] = '\0';
    return j;
}

int asn1::bcd2asc(char* bcd, char* str, int len)
{
#if defined(BCD_AB)
    const char hex_string[] = "0123456789ABCDEF";
#else
    const char hex_string[] = "0123456789*#CDEF";
#endif

    int digit;
    int i = 0;
    int j = 0;

    while (1)
    {
        // Most 4 bits.
        digit = (bcd[i] >> 4) & 0x0F;
        if (digit < 0x0F)
            str[j++] = hex_string[digit];
        else
            break;
        if (j >= len)
            break;
        // Least 4 bits.
        digit = bcd[i++] & 0x0F;
        if (digit < 0x0F)
            str[j++] = hex_string[digit];
        else
            break;
        if (j >= len)
            break;
    }
    str[j] = '\0';
    return j;
}

int asn1::bcd2asc(unsigned char* bcd, char* str, int len)
{
    return bcd2asc(reinterpret_cast<char*>(bcd), str, len);
}

void asn1::trim_alpha(char* number)
{
    char* p;
    char* first;
    char* last;
    for (p = number; *p != '\0'; p++)
    {
        if (isdigit(*p))
            break;
    }
    first = p;
    last = p;
    for (; *p != '\0'; p++)
    {
        if (isdigit(*p))
            last = p + 1;
    }
    *last = '\0';
    if (first != number)
        memcpy(number, first, last - first + 1);
}

int asn1::realtbcd2asc(char* bcd, char* str, int len)
{
    const char hex_string[] = "0123456789ABCDEF";
    int digit;
    int i = 0;
    int j = 0;

    while (1)
    {
        // least 4 bits.
        digit = bcd[i] & 0x0F;
        if (digit < 0x0F)
            str[j++] = hex_string[digit];
        else
            break;
        if (j >= len)
            break;
        // Most 4 bits.
        digit = (bcd[i++] >> 4) & 0x0F;
        if (digit < 0x0F)
            str[j++] = hex_string[digit];
        else
            break;
        if (j >= len)
            break;
    }
    str[j] = '\0';
    return j;
}

int asn1::realbcd2asc(char* bcd, char* str, int len)
{
    const char hex_string[] = "0123456789ABCDEF";
    int digit;
    int i = 0;
    int j = 0;

    while (1)
    {
        // Most 4 bits.
        digit = (bcd[i] >> 4) & 0x0F;
        if (digit < 0x0F)
            str[j++] = hex_string[digit];
        else
            break;
        if (j >= len)
            break;
        // Least 4 bits.
        digit = bcd[i++] & 0x0F;
        if (digit < 0x0F)
            str[j++] = hex_string[digit];
        else
            break;
        if (j >= len)
            break;
    }
    str[j] = '\0';
    return j;
}

int asn1::realbcd2asc(unsigned char* bcd, char* str, int len)
{
    return realbcd2asc(reinterpret_cast<char*>(bcd), str, len);
}

}
}

