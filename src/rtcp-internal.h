//RTCP内部的一些函数定义

#ifndef _rtcp_internal_h_
#define _rtcp_internal_h_

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "rtp-header.h"
#include "rtcp-header.h"
#include "rtp-member.h"
#include "rtp-member-list.h"
#include "rtp.h"



rtp_member* rtp_sender_fetch(rtp_session_t *session, uint32_t ssrc);
rtp_member* rtp_member_fetch(rtp_session_t *session, uint32_t ssrc);

int rtcp_input_rtp(rtp_session_t *session, const void* data, int bytes);
int rtcp_input_rtcp(rtp_session_t *session, const void* data, int bytes);

int rtcp_rr_pack(rtp_session_t *session, uint8_t* data, int bytes);
int rtcp_sr_pack(rtp_session_t *session, uint8_t* data, int bytes);
int rtcp_sdes_pack(rtp_session_t *session, uint8_t* data, int bytes);
int rtcp_bye_pack(rtp_session_t *session, uint8_t* data, int bytes);
int rtcp_app_pack(rtp_session_t *session, uint8_t* ptr, int bytes, const char name[4], const void* app, int len);
void rtcp_rr_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);
void rtcp_sr_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);
void rtcp_sdes_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);
void rtcp_bye_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);
void rtcp_app_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);

int rtcp_report_block(rtp_member* sender, uint8_t* ptr, int bytes);

uint64_t rtpclock(void);
uint64_t ntp2clock(uint64_t ntp);
uint64_t clock2ntp(uint64_t clock);

uint32_t rtp_ssrc(void);

#endif /* !_rtcp_internal_h_ */