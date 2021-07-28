#ifndef _rtp_member_list_h_
#define _rtp_member_list_h_

#include "rtp-member.h"

#define N_SOURCE 2 // unicast(1S + 1R)

//members大小固定为2，ptr不固定
typedef struct 
{
	rtp_member *members[N_SOURCE];//1为sender,2为receiver
	rtp_member **ptr;//
	int count;
	int capacity;
} rtp_member_list;

void* rtp_member_list_create(void);
void rtp_member_list_destroy(void* members);

int rtp_member_list_count(void* members);
rtp_member* rtp_member_list_get(void* members, int index);

rtp_member* rtp_member_list_find(void* members, uint32_t ssrc);

int rtp_member_list_add(void* members, rtp_member* source);
int rtp_member_list_delete(void* members, uint32_t ssrc);

#endif /* !_rtp_member_list_h_ */
