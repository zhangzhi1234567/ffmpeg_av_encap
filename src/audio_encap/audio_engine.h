#ifndef __AUDIO_ENGINE__H_
#define __AUDIO_ENGINE__H_
#ifdef _MSC_VER
#include <windows.h>
#endif
#include <iostream>
#include <map>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/samplefmt.h"
}

using namespace std;
/*
** @brief AudioCapture 音频采集类，需要提供采集的设备名和底层库(windows: dshow ; mac: avfoundation)
** first call audioInit() init Audio param and open device, then call audioCaptureFrame(), get a frame pcm data.
*/
class AudioCapture {
public:
	AudioCapture(string device_name, string lib_name):device_name_(device_name), lib_name_(lib_name),
		packet_(NULL) {}
	~AudioCapture() {}
public:
	int		 audioInit(int channels, AVSampleFormat format, int sample_rate);
	void	 audioDeinit();
	
	int      audioCaptureFrame(AVFrame **frame);
private:
	int		 createFrame(int channel_layout, AVSampleFormat format, int nb_samples);
	int		 audioOpenDevice();
	int		 audioCloseDevice();
	void	 destoryFrame();
private:
	string lib_name_;
	string device_name_;
	AVFormatContext* fmt_ctx_;
	AVFrame *frame_;
	AVPacket *packet_;
	char error[128];
};
/*
** @brief AudioCapture 音频重采样类
** first call audioSampleInit() init Audio param and open device, then call audioSampleConvert(), get a frame pcm data.
*/
class AudioSample {
public:
	AudioSample(int srcRate, AVSampleFormat srcFormat, int srcChLayout, int dstRate, AVSampleFormat dstFormat, int dstChLayout):
				src_rate_(srcRate),
				src_format_(srcFormat),
				src_ch_layout_(srcChLayout),
				dst_rate_(dstRate),
				dst_format_(dstFormat),
				dst_ch_layout_(dstChLayout),
				swr_ctx_(NULL), frame_(NULL){}
	~AudioSample();
public:
	int audioSampleInit();
	int audioSampleConvert(AVFrame *src_frame, AVFrame **dst_frame);
private:
	int createDstFrame(int channel_layout, AVSampleFormat format, int nb_samples);
	int audioSampleCreateData();
private:
	int			   src_rate_;
	AVSampleFormat src_format_;
	int			   src_ch_layout_;
	int			   dst_rate_;
	AVSampleFormat dst_format_;
	int			   dst_ch_layout_;
	SwrContext *swr_ctx_;
	// 准备填充的数据
	uint8_t   **src_data_;
	int         src_len_;
	uint8_t   **dst_data_;
	int         dst_len_;
	AVFrame    *frame_;
};

/*
** @brief AudioEncode 音频编码类 输入frame 输出packet
** first call audioEncodeInit() init Audio encode param and open encoder, then call audioEncode(), get a packet data.
*/


class AudioEncode {
map<int, int> sample_index = {
	{ 96000, 0x0 },{ 88200, 0x1 },{ 64000, 0x2 },{ 48000, 0x3 },{ 44100, 0x4 },{ 32000, 0x5 },
	{ 24000, 0x6 },{ 22050, 0x7 },{ 16000, 0x8 },{ 12000, 0x9 },{ 11025, 0xA },{ 8000 , 0xB }
};
public:
	AudioEncode(string encoderName):encoder_name_(encoderName){}
	~AudioEncode();
public:
	int  audioEncodeInit(AVSampleFormat encode_format, int encode_ch_layout, int sample_rate, int bit_rate, int profile);
	/* encode a packet */
	int  audioEncode(AVFrame *frame, AVPacket **pakcet);

	/* @briedf : 调用 audioEncode 编码完一帧aac 数据后， 需要调用该函数添加 adts 头
	** @aac_buffer: 由 user 提供一个 buffer 长度为 7，头的数据填充到该 buffer 中
	** @frame_len： audioEncode 编码后的一帧数据的长度
	*/
	void packetAddHeader(char *aac_buffer, int frame_len);
	void packetAddHeader(char * aac_header, int profile, int sample_rate, int channels, int frame_len);
private:
	void audio_set_encodec_ctx(AVSampleFormat encode_format, int encode_ch_layout, int sample_rate, int bit_rate, int profile);
private:
	string			encoder_name_;
	AVCodecContext *encodec_ctx_;
	AVPacket        packet_; 
	int				profile_;
	int				channels_; 
	int				sample_rate_;
};

/*
** @brief AudioEncode 音频解码类 输入packet 输出frame
** first call audioDecodeInit() init Audio decode param and open decoder, then call audioDecodePacket(), get a frame data.
*/
class AudioDecode {
public:
	AudioDecode(string lib_name):lib_name_(lib_name), decodec_ctx_(nullptr), frame_(nullptr){}
	~AudioDecode() {
		if (decodec_ctx_) {
			avcodec_close(decodec_ctx_);
			avcodec_free_context(&decodec_ctx_);
		}
		if (frame_)
			av_frame_free(&frame_);
	}
public:
	int audioDecodeInit(int decode_ch_layout, int sample_rate, AVSampleFormat decode_format);
	int audioDecodePacket(AVPacket *src_packet, AVFrame **dst_frame);
private:
	int audioDecodeCreateFrame();

	AVCodecContext *decodec_ctx_;
	AVFrame *frame_;
	string lib_name_;
	int    ch_layout_;
	AVSampleFormat decode_format_;
};


#endif
