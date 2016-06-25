#include "sg_internal.h"

namespace ai
{
namespace sg
{

message_pointer sg_message::create()
{
	sgt_ctx *SGT = sgt_ctx::instance();
	if (SGT->_SGT_cached_msgs.empty())
		return message_pointer(new sg_message(), sg_message_deleter());

	message_pointer msgp = SGT->_SGT_cached_msgs.back();
	SGT->_SGT_cached_msgs.pop_back();
	return msgp;
}

message_pointer sg_message::create(sgt_ctx *SGT)
{
	if (SGT->_SGT_cached_msgs.empty())
		return message_pointer(new sg_message(), sg_message_deleter());

	message_pointer msgp = SGT->_SGT_cached_msgs.back();
	SGT->_SGT_cached_msgs.pop_back();
	return msgp;
}

sg_message::~sg_message()
{
	::free(_buffer);
}

message_pointer sg_message::clone() const
{
	sgt_ctx *SGT = sgt_ctx::instance();
	if (SGT->_SGT_cached_msgs.empty())
		return message_pointer(new sg_message(*this), sg_message_deleter());

	message_pointer msgp = SGT->_SGT_cached_msgs.back();
	SGT->_SGT_cached_msgs.pop_back();
	sg_message *msg = msgp.get();

	msg->reserve(_data_size);
	msg->_data_size = _data_size;
	memcpy(msg->_buffer, _buffer, MSGHDR_LENGTH + _data_size);
	return msgp;
}

void sg_message::encode()
{
	sg_metahdr_t& metahdr = *this;
	metahdr.size = MSGHDR_LENGTH + _data_size;
}

void sg_message::decode()
{
	sg_metahdr_t& metahdr = *this;
	_data_size = metahdr.size - MSGHDR_LENGTH;
}

sg_message& sg_message::operator=(const std::string& __str)
{
	reserve(__str.length());
	_data_size = __str.length();
	memcpy(_data, __str.c_str(), __str.length());
	return *this;
}

sg_message& sg_message::operator=(const char *__s)
{
	size_t len = strlen(__s);
	reserve(len);
	_data_size = len;
	memcpy(_data, __s, len);
	return *this;
}

sg_message& sg_message::operator=(char __c)
{
	resize(1);
	_data_size = 1;
	*_data = __c;
	return *this;
}

size_t sg_message::size() const
{
	return MSGHDR_LENGTH + _data_size;
}

size_t sg_message::length() const
{
	return _data_size;
}

void sg_message::set_size(size_t __size)
{
	_data_size = __size - MSGHDR_LENGTH;
	reserve(_data_size);
}

void sg_message::set_length(size_t __len)
{
	_data_size = __len;
	reserve(_data_size);
}

size_t sg_message::max_size() const
{
	return numeric_limits<size_t>::max();
}

size_t sg_message::raw_capacity() const
{
	return _buffer_size;
}

size_t sg_message::capacity() const
{
	return _buffer_size - MSGHDR_LENGTH;
}

void sg_message::resize(size_t __n, char __c)
{
	if (__n <= _data_size) {
		_data_size = __n;
		return;
	}

	size_t new_len = MSGHDR_LENGTH + __n;
	if (new_len > _buffer_size) {
		_buffer_size = new_len;
		_buffer = reinterpret_cast<char *>(realloc(_buffer, _buffer_size));
		_data = _buffer + MSGHDR_LENGTH;
	}

	memset(_data + _data_size, __c, __n - _data_size);
	_data_size = __n;
}

void sg_message::reserve(size_t __res_arg)
{
	size_t res_size = MSGHDR_LENGTH + __res_arg;
	if (res_size <= _buffer_size)
		return;

	do {
		_buffer_size <<= 1;
	} while (_buffer_size < res_size);

	_buffer = reinterpret_cast<char *>(realloc(_buffer, _buffer_size));
	_data = _buffer + MSGHDR_LENGTH;
}

void sg_message::clear()
{
	_data_size = 0;
}

bool sg_message::empty() const
{
	return (_data_size == 0);
}

const char& sg_message::operator[](size_t __pos) const
{
	return _data[__pos];
}

char& sg_message::operator[](size_t __pos)
{
	return _data[__pos];
}

const char& sg_message::at(size_t __pos) const
{
	if (__pos >= _data_size)
		throw std::out_of_range((_("ERROR: out of range")).str(SGLOCALE));

	return _data[__pos];
}

char& sg_message::at(size_t __pos)
{
	if (__pos >= _data_size)
		throw std::out_of_range((_("ERROR: out of range")).str(SGLOCALE));

	return _data[__pos];
}

sg_message& sg_message::operator+=(const std::string& __str)
{
	size_t new_len = _data_size + __str.length();
	reserve(new_len);
	memcpy(_data + _data_size, __str.c_str(), __str.length());
	_data_size = new_len;
	return *this;
}

sg_message& sg_message::operator+=(const char *__s)
{
	size_t len = strlen(__s);
	size_t new_len = _data_size + len;
	reserve(new_len);
	memcpy(_data + _data_size, __s, len);
	_data_size = new_len;
	return *this;
}

sg_message& sg_message::operator+=(char __c)
{
	size_t new_len = _data_size + 1;
	reserve(new_len);
	_data[_data_size] = __c;
	_data_size = new_len;
	return *this;
}

sg_message& sg_message::append(const std::string& __str)
{
	return (*this += __str);
}

sg_message& sg_message::append(const std::string& __str, size_t __pos, size_t __n)
{
	size_t new_len = _data_size + __n;
	reserve(new_len);
	memcpy(_data + _data_size, __str.c_str() + __pos, __n);
	_data_size = new_len;
	return *this;
}

sg_message& sg_message::append(const char *__s, size_t __n)
{
	size_t new_len = _data_size + __n;
	reserve(new_len);
	memcpy(_data + _data_size, __s, __n);
	_data_size = new_len;
	return *this;
}

sg_message& sg_message::append(const char *__s)
{
	return (*this += __s);
}

sg_message& sg_message::append(size_t __n, char __c)
{
	size_t new_len = _data_size + __n;
	reserve(new_len);
	memset(_data + _data_size, __c, __n);
	_data_size = new_len;
	return *this;
}

sg_message& sg_message::assign(const std::string& __str)
{
	return (*this = __str);
}

sg_message& sg_message::assign(const std::string& __str, size_t __pos, size_t __n)
{
	reserve(__n);
	memcpy(_data, __str.c_str() + __pos, __n);
	_data_size = __n;
	return *this;
}

sg_message& sg_message::assign(const char *__s, size_t __n)
{
	reserve(__n);
	memcpy(_data, __s, __n);
	_data_size = __n;
	return *this;
}

sg_message& sg_message::assign(const char *__s)
{
	return (*this = __s);
}

sg_message& sg_message::assign(size_t __n, char __c)
{
	reserve(__n);
	memset(_data, __c, __n);
	_data_size = __n;
	return *this;
}

void sg_message::swap(sg_message& msg)
{
	std::swap(_buffer_size, msg._buffer_size);
	std::swap(_buffer, msg._buffer);
	std::swap(_data_size, msg._data_size);
	std::swap(_data, msg._data);
}

sg_message::sg_message()
{
	_buffer_size = MSG_MIN;
	_buffer = reinterpret_cast<char *>(malloc(_buffer_size));
	_data_size = 0;
	_data = _buffer + MSGHDR_LENGTH;
}

sg_message::sg_message(const sg_message& rhs)
{
	_buffer_size = rhs._buffer_size;
	_buffer = reinterpret_cast<char *>(malloc(_buffer_size));
	_data_size = rhs._data_size;
	_data = _buffer + MSGHDR_LENGTH;
	memcpy(_buffer, rhs._buffer, MSGHDR_LENGTH + _data_size);
}

ostream& operator<<(ostream& os, const alt_flds_t& flds)
{
	os << "alt_flds: [" << flds.origmid
		<< ", " << flds.origslot
		<< ", " << flds.origtmstmp
		<< ", " << flds.prevbbv
		<< ", " << flds.prevsgv
		<< ", " << flds.newver
		<< ", " << flds.svcsgid
		<< ", " << flds.bbversion
		<< ", " << flds.msgid
		<< "]";

	return os;
}

ostream& operator<<(ostream& os, const sg_metahdr_t& metahdr)
{
	os << "metahdr: [" << metahdr.mtype
		<< ", " << metahdr.protocol
		<< ", " << metahdr.qaddr
		<< ", " << metahdr.mid
		<< ", " << metahdr.size
		<< ", " << metahdr.flags
		<< "]";

	return os;
}

ostream& operator<<(ostream& os, const sg_msghdr_t& msghdr)
{
	os << "msghdr: [" << msghdr.flags
		<< ", " << msghdr.error_code
		<< ", " << msghdr.native_code
		<< ", " << msghdr.ur_code
		<< ", " << msghdr.error_detail
		<< ", " << msghdr.sender
		<< ", " << msghdr.rplyto
		<< ", " << msghdr.rcvr
		<< ", " << msghdr.svc_name
		<< ", " << msghdr.handle
		<< ", " << msghdr.sendmtype
		<< ", " << msghdr.senditer
		<< ", " << msghdr.rplymtype
		<< ", " << msghdr.rplyiter
		<< ", " << msghdr.alt_flds
		<< "]";

	return os;
}

ostream& operator<<(ostream& os, const sg_message& msg)
{
	const sg_metahdr_t& metahdr = msg;
	os << metahdr;

	const sg_msghdr_t& msghdr = msg;
	os << msghdr;

	return os;
}

void sg_message_deleter::operator()(sg_message *msg)
{
	sgp_ctx *SGP = sgp_ctx::instance();
	sgt_ctx *SGT = sgt_ctx::instance();

	if (SGT->exiting()
		|| SGT->_SGT_cached_msgs.size() > SGP->_SGP_max_cached_msgs
		|| msg->raw_capacity() > SGP->_SGP_max_keep_size)
		delete msg;
	else
		SGT->_SGT_cached_msgs.push_back(message_pointer(msg, sg_message_deleter()));
}

}
}

