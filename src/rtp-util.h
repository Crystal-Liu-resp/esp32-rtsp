//大小端读写的转换
#ifndef _rtp_util_h_
#define _rtp_util_h_

#include "rtp-header.h"
#include "rtcp-header.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

// The Internet Protocol defines big-endian as the standard network byte order
// nbo network byte order
#define nbo_r16 rtp_read_uint16
#define nbo_r32 rtp_read_uint32
#define nbo_w16 rtp_write_uint16
#define nbo_w32 rtp_write_uint32

//网络协议使用大端读16bit
static inline uint16_t rtp_read_uint16(const uint8_t* ptr)
{
	return (((uint16_t)ptr[0]) << 8) | ptr[1];
}

//网络协议使用大端读32bit
static inline uint32_t rtp_read_uint32(const uint8_t* ptr)
{
	return (((uint32_t)ptr[0]) << 24) | (((uint32_t)ptr[1]) << 16) | (((uint32_t)ptr[2]) << 8) | ptr[3];
}

//写16bit
static inline void rtp_write_uint16(uint8_t* ptr, uint16_t val)
{
	ptr[0] = (uint8_t)(val >> 8);
	ptr[1] = (uint8_t)val;
}

//写32bit
static inline void rtp_write_uint32(uint8_t* ptr, uint32_t val)
{
	ptr[0] = (uint8_t)(val >> 24);
	ptr[1] = (uint8_t)(val >> 16);
	ptr[2] = (uint8_t)(val >> 8);
	ptr[3] = (uint8_t)val;
}

//写rtp_header
static inline void nbo_write_rtp_header(uint8_t *ptr, const rtp_header_t *header)
{
	ptr[0] = (uint8_t)((header->v << 6) | (header->p << 5) | (header->x << 4) | header->cc);
	ptr[1] = (uint8_t)((header->m << 7) | header->pt);
	ptr[2] = (uint8_t)(header->seq >> 8);
	ptr[3] = (uint8_t)(header->seq & 0xFF);

	nbo_w32(ptr+4, header->timestamp);
	nbo_w32(ptr+8, header->ssrc);
}

//写rtcp_header
static inline void nbo_write_rtcp_header(uint8_t *ptr, const rtcp_hdr_t *header)
{
	ptr[0] = (uint8_t)((header->v << 6) | (header->p << 5) | header->count);
	ptr[1] = (uint8_t)(header->pt);
	ptr[2] = (uint8_t)(header->length >> 8);
	ptr[3] = (uint8_t)(header->length & 0xFF);
}

#endif /* !_rtp_util_h_ */
