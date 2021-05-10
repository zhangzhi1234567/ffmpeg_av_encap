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
** @brief AudioCapture ��Ƶ�ɼ��࣬��Ҫ�ṩ�ɼ����豸���͵ײ��(windows: dshow ; mac: avfoundation)
** first call audioInit() init Audio param and open device, then call audioCaptureFrame(), get a frame pcm data.
*/
class AudioCapture {
public:
	AudioCapture(string device_name, string lib_name):deviceName_(device_name), libName_(lib_name),
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
	string libName_;
	string deviceName_;
	AVFormatContext* fmtCtx_;
	AVFrame *frame_;
	AVPacket *packet_;
	char error[128];
};
/*
** @brief AudioCapture ��Ƶ�ز�����
** first call audioSampleInit() init Audio param and open device, then call audioSampleConvert(), get a frame pcm data.
*/
class AudioSample {
public:
	AudioSample(int srcRate, AVSampleFormat srcFormat, int srcChLayout, int dstRate, AVSampleFormat dstFormat, int dstChLayout):
				srcRate_(srcRate),
				srcFormat_(srcFormat),
				srcChLayout_(srcChLayout),
				dstRate_(dstRate),
				dstFormat_(dstFormat),
				dstChLayout_(dstChLayout),
				swrCtx_(NULL), frame_(NULL){}
	~AudioSample();
public:
	int audioSampleInit();
	int audioSampleConvert(AVFrame *srcFrame, AVFrame **dstFrame);
private:
	int createDstFrame(int channel_layout, AVSampleFormat format, int nb_samples);
	int audioSampleCreateData();
private:
	int			   srcRate_;
	AVSampleFormat srcFormat_;
	int			   srcChLayout_;
	int			   dstRate_;
	AVSampleFormat dstFormat_;
	int			   dstChLayout_;
	SwrContext *swrCtx_;
	// ׼����������
	uint8_t   **srcData_;
	int         srcLen_;
	uint8_t   **dstData_;
	int         dstLen_;
	AVFrame    *frame_;
};

/*
** @brief AudioEncode ��Ƶ������ ����frame ���packet
** first call audioEncodeInit() init Audio encode param and open encoder, then call audioEncode(), get a packet data.
*/


class AudioEncode {
map<int, int> sampleIndex = {
	{ 96000, 0x0 },{ 88200, 0x1 },{ 64000, 0x2 },{ 48000, 0x3 },{ 44100, 0x4 },{ 32000, 0x5 },
	{ 24000, 0x6 },{ 22050, 0x7 },{ 16000, 0x8 },{ 12000, 0x9 },{ 11025, 0xA },{ 8000 , 0xB }
};
public:
	AudioEncode(string encoderName):encoderName_(encoderName){}
	~AudioEncode();
public:
	int  audioEncodeInit(AVSampleFormat encodeFormat, int encodeChLayout, int sampleRate, int bitRate, int profile);
	/* encode a packet */
	int  audioEncode(AVFrame *frame, AVPacket **pakcet);

	/* @briedf : ���� audioEncode ������һ֡aac ���ݺ� ��Ҫ���øú������ adts ͷ
	** @aac_buffer: �� user �ṩһ�� buffer ����Ϊ 7��ͷ��������䵽�� buffer ��
	** @frame_len�� audioEncode ������һ֡���ݵĳ���
	*/
	void packetAddHeader(char *aac_buffer, int frame_len);
	void packetAddHeader(char * aac_header, int profile, int sample_rate, int channels, int frame_len);
private:
	void audio_set_encodec_ctx(AVSampleFormat encodeFormat, int encodeChLayout, int sampleRate, int bitRate, int profile);
private:
	string			encoderName_;
	AVCodecContext *encodecCtx_;
	AVPacket        packet_; 
	int profile_;
	int channels_; 
	int sampleRate_;
};
#endif
