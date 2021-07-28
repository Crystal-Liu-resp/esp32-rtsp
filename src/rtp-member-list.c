#include "rtp-member-list.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

void* rtp_member_list_create()
{
	return (rtp_member_list *)calloc(1, sizeof(rtp_member_list));
}

void rtp_member_list_destroy(void* members)
{
	int i;
	rtp_member_list *p;
	p = (rtp_member_list *)members;

	for(i = 0; i < p->count; i++)
	{
		rtp_member_release(i >= N_SOURCE ? p->ptr[i-N_SOURCE] : p->members[i]);
	}

	if(p->ptr)
	{
		assert(p->capacity > 0);
		free(p->ptr);
	}

	free(p);
}

//返回member数量
int rtp_member_list_count(void* members)
{
	rtp_member_list *p;
	p = (rtp_member_list *)members;
	return p->count;
}

//返回特定的某一个member
rtp_member* rtp_member_list_get(void* members, int index)
{
	rtp_member_list *p;
	p = (rtp_member_list *)members;
	if(index >= p->count || index < 0)
		return NULL;

	return index >= N_SOURCE ? p->ptr[index-N_SOURCE] : p->members[index];//不止1S+1R，返回前面的，否则返回后面的
}

//返回由ssrc决定的特定的某一个member
rtp_member* rtp_member_list_find(void* members, uint32_t ssrc)
{
	int i;
	rtp_member *s;
	rtp_member_list *p;
	p = (rtp_member_list *)members;

	for(i = 0; i < p->count; i++)
	{
		s = i < N_SOURCE ? p->members[i] : p->ptr[i-N_SOURCE];
		if(s->ssrc == ssrc)
			return s;
	}
	return NULL;
}

//添加member到list里面
int rtp_member_list_add(void* members, rtp_member* s)
{
	rtp_member_list *p;
	p = (rtp_member_list *)members;

	if(p->count >= N_SOURCE)//不止1S+1R，加到p->ptr中
	{
		if(p->count - N_SOURCE >= p->capacity)//超过capacity，重新分配内存空间
		{
			void* ptr;
			ptr = (rtp_member **)realloc(p->ptr, (p->capacity+8)*sizeof(rtp_member*));//重新分配内存， 8？？？
			if(!ptr)
				return ENOMEM;
			p->ptr = ptr;
			p->capacity += 8;
		}
		p->ptr[p->count - N_SOURCE] = s;//没超过capacity
	}
	else//将rtp_member* s加到list的members中，s是1S+1R之一
	{
		p->members[p->count] = s;
	}

	rtp_member_addref(s);//返回member存在member_list中的参考地址
	p->count++;
	return 0;
}

int rtp_member_list_delete(void* members, uint32_t ssrc)
{
	int i;
	rtp_member *s;
	rtp_member_list *p;
	p = (rtp_member_list *)members;

	for(i = 0; i < p->count; i++)
	{
		s = i < N_SOURCE ? p->members[i] : p->ptr[i-N_SOURCE];
		if(s->ssrc != ssrc)
			continue;

		if(i < N_SOURCE)
		{
			if(i+1 < N_SOURCE)
			{
				memmove(p->members + i, p->members+i+1, (N_SOURCE-i-1) * sizeof(rtp_member*));
			}

			if(p->count > N_SOURCE)
			{
				p->members[N_SOURCE-1] = p->ptr[0];
				memmove(p->ptr, p->ptr + 1, (p->count-N_SOURCE-1) * sizeof(rtp_member*));
			}
		}
		else
		{
			if(i + 1 < p->count)
			{
				memmove(p->ptr + i - N_SOURCE, p->ptr + i + 1 - N_SOURCE, (p->count-i-1) * sizeof(rtp_member*));
			}
		}

		rtp_member_release(s);
		p->count--;
		return 0;
	}

	return -1; // NOT_FOUND
}
