#include "audio_engine.h"

int AudioCapture::audioInit(int channel_layout, AVSampleFormat format, int sample_rate)
{
	error[128] = { 0 };
	av_register_all();
	avdevice_register_all();
	av_log_set_level(AV_LOG_DEBUG);
	packet_ = av_packet_alloc();
	av_init_packet(packet_);
	int ret = createFrame(channel_layout, format, sample_rate);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "create frame failure.\n");
		return -1;
	}
	ret = audioOpenDevice();
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "create frame failure.\n");
		return -1;
	}
	return 0;
}
void AudioCapture::audioDeinit()
{
	audioCloseDevice();
	destoryFrame();
	av_packet_free(&packet_);
}
void AudioCapture::destoryFrame()
{
	av_frame_unref(frame_);
}
int AudioCapture::audioCloseDevice()
{
	if (fmtCtx_)
	{
		avformat_close_input(&fmtCtx_);
	}

	return 0;
}
int AudioCapture::createFrame(int channel_layout, AVSampleFormat format, int nb_samples)
{
	frame_ = av_frame_alloc();
	frame_->channel_layout = channel_layout;
	frame_->format = format;
	frame_->nb_samples = nb_samples;
	int ret = av_frame_get_buffer(frame_, 0);
	if (ret != 0){
		av_strerror(ret, error, 128);
		av_log(NULL, AV_LOG_ERROR, "open input failure.[%d][%s]\n", AVERROR(ret), error);
		return -1;
	}	
	return 0;
}

int AudioCapture::audioOpenDevice()
{
	AVInputFormat *inputFmt = av_find_input_format(libName_.c_str());
	if (!inputFmt) {
		av_log(NULL, AV_LOG_ERROR, "find lib %s failure.\n", libName_.c_str());
		return -1;
	}

	int ret = avformat_open_input(&fmtCtx_, deviceName_.c_str(), inputFmt, NULL);
	if (ret != 0) {
		av_strerror(ret, error, 128);
		av_log(NULL, AV_LOG_ERROR, "open input failure.[ret][%s]\n", AVERROR(ret), error);
		return -1;
	}
	av_dump_format(fmtCtx_, 0, deviceName_.c_str(), 0);
	return 0;
}

int AudioCapture::audioCaptureFrame(AVFrame **frame)
{
	static int read_size = -1;
	static int write_size = 0;
	//���һƬ��2184, �����ֻ�е�һ�ζ���ʱ�򣬻�� while
	while(read_size < 0) {
		int ret = av_read_frame(fmtCtx_, packet_);
		if (ret != 0) {
			return ret;
		}
		printf("read packet.size = %d, read_size = %d\n", packet_->size, read_size);
		//av_packet_unref(packet_);
		read_size = packet_->size;
		write_size = 0;
	} 
	if (read_size < 4096) { //���һƬ�� 2184
		memcpy(frame_->data[0], packet_->data + write_size, read_size);
		av_read_frame(fmtCtx_, packet_); //����һ֡ʱ����ȡ packet ���ݣ�������� frame->data[0] �������� 4096 - read_size
		printf("last read_size = %d\n", read_size);
		int remain_len = 4096 - read_size;
		memcpy(frame_->data[0] + read_size, packet_->data, remain_len);
		read_size = packet_->size - remain_len; //������ packet �� read_size �� write_size
		write_size = remain_len;
	} else {
		memcpy(frame_->data[0], packet_->data + write_size, 4096);
		read_size -= 4096;
		write_size += 4096;
	}
	
	//frame_->data[0]�Ĵ�С�ǲ������Ƶģ���ŵ���һ֡����������
	//frame_->linesize[0]�������С��
    // pkt.size = 88200�������˺ü�֡�����ݣ�ֱ����data[0]������Խ��.
	*frame = frame_;
	return 0;
}




/////////////////////////// AudioSample �ز�����ʵ��///////////////////////////////////////////////////////////////
AudioSample::~AudioSample()
{
	if (srcData_)
		av_freep(&srcData_[0]);
	av_freep(srcData_);
	if (dstData_)
		av_freep(&dstData_[0]);
	av_freep(dstData_);
	swr_free(&swrCtx_);
}

int AudioSample::audioSampleInit()
{
	swrCtx_ = swr_alloc_set_opts(NULL,
		dstChLayout_, dstFormat_, dstRate_,
		srcChLayout_, srcFormat_, srcRate_,
		0, NULL);
	if (!swrCtx_) {
		av_log(NULL, AV_LOG_ERROR, "create swr ctx fail.\n");
		return -1;
	}
	swr_init(swrCtx_);
	audioSampleCreateData();
	
	return 0;
}

int AudioSample::createDstFrame(int channel_layout, AVSampleFormat format, int nb_samples)
{
	if (frame_) 
		return 0;
	frame_ = av_frame_alloc();
	frame_->channel_layout = channel_layout;
	frame_->format = format;
	frame_->nb_samples = nb_samples;
	int ret = av_frame_get_buffer(frame_, 0);
	if (ret != 0) {
		av_log(NULL, AV_LOG_ERROR, "open input failure.[%d][%s]\n", AVERROR(ret));
		return -1;
	}
	return 0;
}

int AudioSample::audioSampleCreateData()
{
	// �����ز������������
	av_samples_alloc_array_and_samples(
		&dstData_, //�����������ַ
		&dstLen_, //��������С
		av_get_channel_layout_nb_channels(dstChLayout_), //ͨ������ av_get_channel_layout_nb_channels(ch_layout)
		1024,    //�������� 4096(�ֽ�)/2(����λ��16 bit)/2ͨ��
		dstFormat_, //������ʽ ���������������������Ҫ�����ֽ�, �� �ɼ���ʱ�� frame->data ��ĳ���һ��
		0);
	
	// �����ز������뻺����
	av_samples_alloc_array_and_samples(
		&srcData_, //�����������ַ
		&srcLen_, //��������С
		av_get_channel_layout_nb_channels(srcChLayout_), //ͨ������ av_get_channel_layout_nb_channels(ch_layout)
		1024,    //�������� 4096(�ֽ�)/2(����λ��16 bit)/2ͨ��
		srcFormat_, //������ʽ ���������������������Ҫ�����ֽ�, �� �ɼ���ʱ�� frame->data ��ĳ���һ��
		0);
	printf("create sample dst data len = %d\n", dstLen_);
	printf("create sample src data len = %d\n", srcLen_);
	return 0;
}

int AudioSample::audioSampleConvert(AVFrame *srcFrame, AVFrame **dstFrame)
{
	memcpy((void *)srcData_[0], srcFrame->data[0], srcFrame->linesize[0]);
	int nb_samples = swr_convert(swrCtx_, dstData_, 1024, (const uint8_t **)srcData_, 1024);
	int dst_linesize = 0;
	int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, av_get_channel_layout_nb_channels(dstChLayout_),
		nb_samples, dstFormat_, 1); //�ز����� FLTP ��
	
	printf("convert dst_bufsize = %d, dst_linesize = %d, nb_samples = %d\n", dst_bufsize, dst_linesize, nb_samples);
	
	createDstFrame(dstChLayout_, dstFormat_, nb_samples);
	printf("convert create dst frame[0] size = %d\n", frame_->linesize[0]);
	int planar = av_sample_fmt_is_planar(dstFormat_);
	if (planar) {
		memcpy(frame_->data[0], dstData_[0], frame_->linesize[0]);
		memcpy(frame_->data[1], dstData_[0] + frame_->linesize[0], frame_->linesize[0]);
	} else {
		memcpy(frame_->data[0], dstData_[0], frame_->linesize[0]);
	}
	
	*dstFrame = frame_;
	return 0;
}

AudioEncode::~AudioEncode()
{
	avcodec_free_context(&encodecCtx_);
}

int AudioEncode::audioEncodeInit(AVSampleFormat encodeFormat, int encodeChLayout, int sampleRate, int bitRate, int profile)
{
	AVCodec *codec = avcodec_find_encoder_by_name(encoderName_.c_str());
	if (!codec) {
		av_log(NULL, AV_LOG_ERROR, "audio not find encoder : %s\n", encoderName_.c_str());
		return -1;
	}
	encodecCtx_ = avcodec_alloc_context3(codec);
	if (!encodecCtx_) {
		av_log(NULL, AV_LOG_ERROR, "avcodec alloc ctx failed.\n");
		return -1;
	}

	audio_set_encodec_ctx(encodeFormat, encodeChLayout, sampleRate, bitRate, profile);

	int ret = avcodec_open2(encodecCtx_, codec, NULL);
	if (ret != 0) {
		av_log(NULL, AV_LOG_ERROR, "avcodec open 2 failed.\n");
		return -1;
	}
	av_init_packet(&packet_);
	profile_ = profile;
	channels_ = av_get_channel_layout_nb_channels(encodeChLayout);
	sampleRate_ = sampleRate;

	return 0;
}

int AudioEncode::audioEncode(AVFrame *frame, AVPacket **packet)
{
	int ret = avcodec_send_frame(encodecCtx_, frame);
	//ret >= 0˵���������óɹ���
	while (ret >= 0) {
		ret = avcodec_receive_packet(encodecCtx_, &packet_);
		//Ҫһֱ��ȡ֪��ʧ�ܣ���Ϊ���ܻ��кö�֡��Ҫ�³�����
		if (ret < 0) {
			//˵��û��֡�ˣ�Ҫ����������
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			}
			else {
				return -1;//������ֵ��˵����ʧ���ˡ�
			}
		}
		break;  //��ȡ��һ��packet
	}
	*packet = &packet_;
	return 0;
}

/* |- syncword(12) ...             | ID(v)(1)|    layer(2)  | protection_absent(1)|  (16bit)
** |- profile(2)   | private_bit(1)| sample_rate_index(4) | channel_nb(1)|(8bit) //ͨ����buf[2]ֻ����1bit,ʣ�µ�2bit��buf[3]
** | -channel_nb(2)|

*/
void AudioEncode::packetAddHeader(char *aac_header, int frame_len)
{
	int sampling_frequency_index = sampleIndex.at(sampleRate_);
	printf("add header sample rate :%d, index: %d\n", sampleRate_, sampling_frequency_index);
	//sync word
	aac_header[0] = 0xff;         //syncword:0xfff                          ��8bits
	aac_header[1] = 0xf0;         //syncword:0xfff                          ��4bits
	//ID
	aac_header[1] |= (0 << 3);    //ID : 0 for MPEG-4 ;  1 for MPEG-2  
	//layer
	aac_header[1] |= (0 << 1);    //Layer:0  always:00
	//protection absent
	aac_header[1] |= 1;           //protection absent:1    ǰ�����ֽڵ����λΪ 1 
	// also : aac_header[0] = 0xff; aac_header[1] = 0xf1;
	//profile
	aac_header[2] = (profile_) << 6;  //profile:profile  2bits
	//sampling_frequency_index
	aac_header[2] |= (sampling_frequency_index & 0x0f) << 2; //sampling_frequency_index 4bits ֻ��4bitҪ &0x0f����ո�4λ 
	//private_bit
	aac_header[2] |= (0 << 1);        //private_bit: 0   1bits      
	//channels
	aac_header[2] |= (channels_ & 0x04) >> 2;  //channel ��1bit: &0000 0100ȡchannels�����λ
	aac_header[3] |= (channels_ & 0x03) << 6;  //&0000 0011ȡchannels�ĵ�2λ

	aac_header[3] |= (0 << 5);               //original��0                1bit
	aac_header[3] |= (0 << 4);               //home��0                    1bit
	aac_header[3] |= (0 << 3);               //copyright id bit��0        1bit
	aac_header[3] |= (0 << 2);               //copyright id start��0      1bit

	//frame len һ��ADTS֡�ĳ��Ȱ���ADTSͷ��AACԭʼ��
	int adtsLen = frame_len + 7;
	aac_header[3] |= ((adtsLen & 0x1800) >> 11);           //frame length��value   ��2bits 
	aac_header[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    �м�8bits
	aac_header[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    ��3bits
	//buffer fullness 0x7FF ˵�������ʿɱ������
	aac_header[5] |= 0x1f;                                 //buffer fullness:0x7ff ��5bits
	aac_header[6] = 0xfc;      //?11111100?                  //buffer fullness:0x7ff ��6bits
	return ;
}
void AudioEncode::packetAddHeader(char *aac_header, int profile, int sample_rate, int channels, int frame_len)
{
	int sampling_frequency_index = sampleIndex.at(sample_rate);
	printf("add header sample rate :%d, index: %d\n", sample_rate, sampling_frequency_index);
	//sync word
	aac_header[0] = 0xff;         //syncword:0xfff                          ��8bits
	aac_header[1] = 0xf0;         //syncword:0xfff                          ��4bits
								  //ID
	aac_header[1] |= (0 << 3);    //ID : 0 for MPEG-4 ;  1 for MPEG-2  
								  //layer
	aac_header[1] |= (0 << 1);    //Layer:0  always:00
								  //protection absent
	aac_header[1] |= 1;           //protection absent:1    ǰ�����ֽڵ����λΪ 1 
								  // also : aac_header[0] = 0xff; aac_header[1] = 0xf1;
								  //profile
	aac_header[2] = (profile) << 6;  //profile:profile  2bits
									  //sampling_frequency_index
	aac_header[2] |= (sampling_frequency_index & 0x0f) << 2; //sampling_frequency_index 4bits ֻ��4bitҪ &0x0f����ո�4λ 
															 //private_bit
	aac_header[2] |= (0 << 1);        //private_bit: 0   1bits      
									  //channels
	aac_header[2] |= (channels & 0x04) >> 2;  //channel ��1bit: &0000 0100ȡchannels�����λ
	aac_header[3] |= (channels & 0x03) << 6;  //&0000 0011ȡchannels�ĵ�2λ

	aac_header[3] |= (0 << 5);               //original��0                1bit
	aac_header[3] |= (0 << 4);               //home��0                    1bit
	aac_header[3] |= (0 << 3);               //copyright id bit��0        1bit
	aac_header[3] |= (0 << 2);               //copyright id start��0      1bit

											 //frame len һ��ADTS֡�ĳ��Ȱ���ADTSͷ��AACԭʼ��
	int adtsLen = frame_len + 7;
	aac_header[3] |= ((adtsLen & 0x1800) >> 11);           //frame length��value   ��2bits 
	aac_header[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    �м�8bits
	aac_header[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    ��3bits
														   //buffer fullness 0x7FF ˵�������ʿɱ������
	aac_header[5] |= 0x1f;                                 //buffer fullness:0x7ff ��5bits
	aac_header[6] = 0xfc;      //?11111100?                  //buffer fullness:0x7ff ��6bits
	return;
}
void AudioEncode::audio_set_encodec_ctx(AVSampleFormat encodeFormat, int encodeChLayout,
										 int sampleRate, int bitRate, int profile)
{
	encodecCtx_->sample_fmt = encodeFormat;
	encodecCtx_->channel_layout = encodeChLayout;
	encodecCtx_->sample_rate = sampleRate;
	encodecCtx_->bit_rate = bitRate;
	encodecCtx_->profile = profile;
	encodecCtx_->channels = av_get_channel_layout_nb_channels(encodeChLayout);
}



