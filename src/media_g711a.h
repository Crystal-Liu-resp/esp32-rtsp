

#ifndef _MEDIA_G711A_H_
#define _MEDIA_G711A_H_

#include "MediaSource.h"
#include "rtp.h"



class G711ASource : public MediaSource
{
public:
	static G711ASource* CreateNew();
	virtual ~G711ASource();

	uint32_t GetSampleRate() const
	{ return samplerate_; }

	uint32_t GetChannels() const
	{ return channels_; }

	virtual std::string GetMediaDescription(uint16_t port=0);

	virtual std::string GetAttribute();

	bool HandleFrame(MediaChannelId channel_id, AVFrame frame);

	static uint32_t GetTimestamp();

private:
	G711ASource();

	uint32_t samplerate_ = 8000;   
	uint32_t channels_ = 1;       
};

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif
