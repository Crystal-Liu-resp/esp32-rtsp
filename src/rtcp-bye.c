// RFC3550 6.6 BYE: Goodbye RTCP Packet

#include "rtcp-internal.h"
#include "rtp-util.h"

// void rtcp_bye_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* ptr)
// {
// 	uint32_t i;
// 	struct rtcp_msg_t msg;

// 	assert(header->length * 4 >= header->count * 4);
// 	if(header->count < 1 || header->count > header->length)
// 		return; // A count value of zero is valid, but useless (p43)

// 	msg.type = RTCP_MSG_BYE;
// 	if(header->length * 4 > header->count * 4)
// 	{
// 		msg.u.bye.bytes = ptr[header->count * 4];
// 		msg.u.bye.reason = ptr + header->count * 4 + 1;

// 		if (1 + msg.u.bye.bytes + header->count * 4 > header->length * 4)
// 		{
// 			assert(0);
// 			return; // error
// 		}
// 	}
// 	else
// 	{
// 		msg.u.bye.bytes = 0;
// 		msg.u.bye.reason = NULL;
// 	}

// 	for(i = 0; i < header->count; i++)
// 	{
// 		msg.u.bye.ssrc = nbo_r32(ptr + i * 4);
// 		rtp_member_list_delete(session->members, msg.u.bye.ssrc);
// 		rtp_member_list_delete(session->senders, msg.u.bye.ssrc);

// 		session->handler.on_rtcp(session->cbparam, &msg);
// 	}
// }

// int rtcp_bye_pack(rtp_session_t *session, uint8_t* ptr, int bytes)
// {
// 	rtcp_hdr_t header;

// 	if(bytes < 8)
// 		return 8;

// 	header.v = 2;
// 	header.p = 0;
// 	header.pt = RTCP_BYE;
// 	header.rc = 1; // self only
// 	header.length = 1;
// 	nbo_write_rtcp_header(ptr, &header);

// 	nbo_w32(ptr+4, session->self->ssrc);

// 	assert(8 == (header.length+1)*4);
// 	return 8;
// }
