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
	if (fmt_ctx_) {
		avformat_close_input(&fmt_ctx_);
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
	AVInputFormat *inputFmt = av_find_input_format(lib_name_.c_str());
	if (!inputFmt) {
		av_log(NULL, AV_LOG_ERROR, "find lib %s failure.\n", lib_name_.c_str());
		return -1;
	}

	int ret = avformat_open_input(&fmt_ctx_, device_name_.c_str(), inputFmt, NULL);
	if (ret != 0) {
		av_strerror(ret, error, 128);
		av_log(NULL, AV_LOG_ERROR, "open input failure.[ret][%s]\n", AVERROR(ret), error);
		return -1;
	}
	av_dump_format(fmt_ctx_, 0, device_name_.c_str(), 0);
	return 0;
}

int AudioCapture::audioCaptureFrame(AVFrame **frame)
{
	static int read_size = -1;
	static int write_size = 0;
	//最后一片是2184, 这里就只有第一次读的时候，会进 while
	while(read_size < 0) {
		int ret = av_read_frame(fmt_ctx_, packet_);
		if (ret != 0) {
			return ret;
		}
		printf("read packet.size = %d, read_size = %d\n", packet_->size, read_size);
		//av_packet_unref(packet_);
		read_size = packet_->size;
		write_size = 0;
	} 
	if (read_size < 4096) { //最后一片是 2184
		memcpy(frame_->data[0], packet_->data + write_size, read_size);
		av_read_frame(fmt_ctx_, packet_); //不够一帧时，读取 packet 数据，继续填充 frame->data[0] ，长度是 4096 - read_size
		printf("last read_size = %d\n", read_size);
		int remain_len = 4096 - read_size;
		memcpy(frame_->data[0] + read_size, packet_->data, remain_len);
		read_size = packet_->size - remain_len; //重置新 packet 的 read_size 和 write_size
		write_size = remain_len;
	} else {
		memcpy(frame_->data[0], packet_->data + write_size, 4096);
		read_size -= 4096;
		write_size += 4096;
	}
	
	//frame_->data[0]的大小是参数控制的，存放的是一帧的数据量，
	//frame_->linesize[0]就是其大小，
    // pkt.size = 88200，包括了好几帧的数据，直接往data[0]里拷贝会越界.
	*frame = frame_;
	return 0;
}




/////////////////////////// AudioSample 重采样类实现///////////////////////////////////////////////////////////////
AudioSample::~AudioSample()
{
	if (src_data_)
		av_freep(&src_data_[0]);
	av_freep(src_data_);
	if (dst_data_)
		av_freep(&dst_data_[0]);
	av_freep(dst_data_);
	swr_free(&swr_ctx_);
	if (frame_)
		av_frame_free(&frame_);
}

int AudioSample::audioSampleInit()
{
	swr_ctx_ = swr_alloc_set_opts(NULL,
		dst_ch_layout_, dst_format_, dst_rate_,
		src_ch_layout_, src_format_, src_rate_,
		0, NULL);
	if (!swr_ctx_) {
		av_log(NULL, AV_LOG_ERROR, "create swr ctx fail.\n");
		return -1;
	}
	swr_init(swr_ctx_);
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
	// 创建重采样输出缓冲区
	av_samples_alloc_array_and_samples(
		&dst_data_, //输出缓冲区地址
		&dst_len_, //缓冲区大小
		av_get_channel_layout_nb_channels(dst_ch_layout_), //通道个数 av_get_channel_layout_nb_channels(ch_layout)
		1024,    //采样个数 4096(字节)/2(采样位数16 bit)/2通道
		dst_format_, //采样格式 根据这三个就能算出来需要的总字节, 和 采集的时候 frame->data 算的长度一致
		0);
	
	// 创建重采样输入缓冲区
	av_samples_alloc_array_and_samples(
		&src_data_, //输出缓冲区地址
		&src_len_, //缓冲区大小
		av_get_channel_layout_nb_channels(src_ch_layout_), //通道个数 av_get_channel_layout_nb_channels(ch_layout)
		1024,    //采样个数 4096(字节)/2(采样位数16 bit)/2通道
		src_format_, //采样格式 根据这三个就能算出来需要的总字节, 和 采集的时候 frame->data 算的长度一致
		0);
	printf("create sample dst data len = %d\n", dst_len_);
	printf("create sample src data len = %d\n", src_len_);
	return 0;
}

int AudioSample::audioSampleConvert(AVFrame *src_frame, AVFrame **dst_frame)
{
	// 往源缓存区拷贝数据 需要判断 srcFrame 是plannar 还是 packetd
	int planar = av_sample_fmt_is_planar(src_format_);
	if (planar) {
		memcpy((void *)src_data_[0], src_frame->data[0], src_frame->linesize[0]/2);
		memcpy((void *)(src_data_[0] + src_frame->linesize[0]/2), src_frame->data[1], src_frame->linesize[0]/2);
	}
	else {
		memcpy((void *)src_data_[0], src_frame->data[0], src_frame->linesize[0]);
	}
	
	int nb_samples = swr_convert(swr_ctx_, dst_data_, 1024, (const uint8_t **)src_data_, 1024);
	int dst_linesize = 0;
	int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, av_get_channel_layout_nb_channels(dst_ch_layout_),
		nb_samples, dst_format_, 1); //重采样成 FLTP 了
	
	printf("convert dst_bufsize = %d, dst_linesize = %d, nb_samples = %d\n", dst_bufsize, dst_linesize, nb_samples);
	
	createDstFrame(dst_ch_layout_, dst_format_, nb_samples);
	printf("convert create dst frame[0] size = %d\n", frame_->linesize[0]);
	
	// 从目的缓存区 往dstFrame拷贝数据 需要判断 dstFrame 是plannar 还是 packetd
	planar = av_sample_fmt_is_planar(dst_format_);
	if (planar) {
		memcpy(frame_->data[0], dst_data_[0], frame_->linesize[0]);
		memcpy(frame_->data[1], dst_data_[0] + frame_->linesize[0], frame_->linesize[0]);
	} else {
		memcpy(frame_->data[0], dst_data_[0], frame_->linesize[0]);
	}
	
	*dst_frame = frame_;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//////////////////////audio encode/////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
AudioEncode::~AudioEncode()
{
	avcodec_close(encodec_ctx_);
	avcodec_free_context(&encodec_ctx_);
}

int AudioEncode::audioEncodeInit(AVSampleFormat encode_format, int encode_ch_layout, int sample_rate, int bit_rate, int profile)
{
	AVCodec *codec = avcodec_find_encoder_by_name(encoder_name_.c_str());
	if (!codec) {
		av_log(NULL, AV_LOG_ERROR, "audio not find encoder : %s\n", encoder_name_.c_str());
		return -1;
	}
	encodec_ctx_ = avcodec_alloc_context3(codec);
	if (!encodec_ctx_) {
		av_log(NULL, AV_LOG_ERROR, "avcodec alloc ctx failed.\n");
		return -1;
	}

	audio_set_encodec_ctx(encode_format, encode_ch_layout, sample_rate, bit_rate, profile);

	int ret = avcodec_open2(encodec_ctx_, codec, NULL);
	if (ret != 0) {
		av_log(NULL, AV_LOG_ERROR, "avcodec open 2 failed.\n");
		return -1;
	}
	av_init_packet(&packet_);
	profile_ = profile;
	channels_ = av_get_channel_layout_nb_channels(encode_ch_layout);
	sample_rate_ = sample_rate;

	return 0;
}

int AudioEncode::audioEncode(AVFrame *frame, AVPacket **packet)
{
	int ret = avcodec_send_frame(encodec_ctx_, frame);
	//ret >= 0说明数据设置成功了
	while (ret >= 0) {
		ret = avcodec_receive_packet(encodec_ctx_, &packet_);
		//要一直获取知道失败，因为可能会有好多帧需要吐出来，
		if (ret < 0) {
			//说明没有帧了，要继续送数据
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				return EAGAIN;
			}
			else {
				return -1;//其他的值，说明真失败了。
			}
		}
		break;  //获取到一个packet
	}
	*packet = &packet_;
	return 0;
}

/* |- syncword(12) ...             | ID(v)(1)|    layer(2)  | protection_absent(1)|  (16bit)
** |- profile(2)   | private_bit(1)| sample_rate_index(4) | channel_nb(1)|(8bit) //通道数buf[2]只填了1bit,剩下的2bit在buf[3]
** | -channel_nb(2)|

*/
void AudioEncode::packetAddHeader(char *aac_header, int frame_len)
{
	int sampling_frequency_index = sample_index.at(sample_rate_);
	printf("add header sample rate :%d, index: %d\n", sample_rate_, sampling_frequency_index);
	//sync word
	aac_header[0] = 0xff;         //syncword:0xfff                          高8bits
	aac_header[1] = 0xf0;         //syncword:0xfff                          低4bits
	//ID
	aac_header[1] |= (0 << 3);    //ID : 0 for MPEG-4 ;  1 for MPEG-2  
	//layer
	aac_header[1] |= (0 << 1);    //Layer:0  always:00
	//protection absent
	aac_header[1] |= 1;           //protection absent:1    前两个字节的最低位为 1 
	// also : aac_header[0] = 0xff; aac_header[1] = 0xf1;
	//profile
	aac_header[2] = (profile_ + 1) << 6;  //profile:profile  2bits
	//sampling_frequency_index
	aac_header[2] |= (sampling_frequency_index & 0x0f) << 2; //sampling_frequency_index 4bits 只有4bit要 &0x0f，清空高4位 
	//private_bit
	aac_header[2] |= (0 << 1);        //private_bit: 0   1bits      
	//channels
	aac_header[2] |= (channels_ & 0x04) >> 2;  //channel 高1bit: &0000 0100取channels的最高位
	aac_header[3] |= (channels_ & 0x03) << 6;  //&0000 0011取channels的低2位

	aac_header[3] |= (0 << 5);               //original：0                1bit
	aac_header[3] |= (0 << 4);               //home：0                    1bit
	aac_header[3] |= (0 << 3);               //copyright id bit：0        1bit
	aac_header[3] |= (0 << 2);               //copyright id start：0      1bit

	//frame len 一个ADTS帧的长度包括ADTS头和AAC原始流
	int adtsLen = frame_len + 7;
	aac_header[3] |= ((adtsLen & 0x1800) >> 11);           //frame length：value   高2bits 
	aac_header[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    中间8bits
	aac_header[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    低3bits
	//buffer fullness 0x7FF 说明是码率可变的码流
	aac_header[5] |= 0x1f;                                 //buffer fullness:0x7ff 高5bits
	aac_header[6] = 0xfc;      //?11111100?                  //buffer fullness:0x7ff 低6bits
	return ;
}
void AudioEncode::packetAddHeader(char *aac_header, int profile, int sample_rate, int channels, int frame_len)
{
	int sampling_frequency_index = sample_index.at(sample_rate);
	printf("add header sample rate :%d, index: %d\n", sample_rate, sampling_frequency_index);
	//sync word
	aac_header[0] = 0xff;         //syncword:0xfff                          高8bits
	aac_header[1] = 0xf0;         //syncword:0xfff                          低4bits
								  //ID
	aac_header[1] |= (0 << 3);    //ID : 0 for MPEG-4 ;  1 for MPEG-2  
								  //layer
	aac_header[1] |= (0 << 1);    //Layer:0  always:00
								  //protection absent
	aac_header[1] |= 1;           //protection absent:1    前两个字节的最低位为 1 
								  // also : aac_header[0] = 0xff; aac_header[1] = 0xf1;
								  //profile
	aac_header[2] = (profile) << 6;  //profile:profile  2bits
									  //sampling_frequency_index
	aac_header[2] |= (sampling_frequency_index & 0x0f) << 2; //sampling_frequency_index 4bits 只有4bit要 &0x0f，清空高4位 
															 //private_bit
	aac_header[2] |= (0 << 1);        //private_bit: 0   1bits      
									  //channels
	aac_header[2] |= (channels & 0x04) >> 2;  //channel 高1bit: &0000 0100取channels的最高位
	aac_header[3] |= (channels & 0x03) << 6;  //&0000 0011取channels的低2位

	aac_header[3] |= (0 << 5);               //original：0                1bit
	aac_header[3] |= (0 << 4);               //home：0                    1bit
	aac_header[3] |= (0 << 3);               //copyright id bit：0        1bit
	aac_header[3] |= (0 << 2);               //copyright id start：0      1bit

											 //frame len 一个ADTS帧的长度包括ADTS头和AAC原始流
	int adtsLen = frame_len + 7;
	aac_header[3] |= ((adtsLen & 0x1800) >> 11);           //frame length：value   高2bits 
	aac_header[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    中间8bits
	aac_header[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    低3bits
														   //buffer fullness 0x1FF 说明是码率可变的码流
	aac_header[5] |= 0x1f;                                 //buffer fullness:0x1ff 高5bits
	aac_header[6] = 0xfc;      //111111 00                  //buffer fullness:0x1ff 低6bits
	//buffer fullness = 1FF, number_of_raw_data_blocks_in_frame = 0, 表示说ADTS帧中有一个AAC数据块
	return;
}
void AudioEncode::audio_set_encodec_ctx(AVSampleFormat encode_format, int encode_ch_layout,
										 int sample_rate, int bit_rate, int profile)
{
	encodec_ctx_->sample_fmt = encode_format;
	encodec_ctx_->channel_layout = encode_ch_layout;
	encodec_ctx_->sample_rate = sample_rate;
	encodec_ctx_->bit_rate = bit_rate;
	encodec_ctx_->profile = profile;
	encodec_ctx_->channels = av_get_channel_layout_nb_channels(encode_ch_layout);
}

int AudioDecode::audioDecodeInit(int decode_ch_layout, int sample_rate, AVSampleFormat decode_format)
{
	av_register_all();
	AVCodec *avodec = avcodec_find_decoder_by_name(lib_name_.c_str());
	if (!avodec) {
		av_log(NULL, AV_LOG_ERROR, "audio decode find decoder %s failed.\n", lib_name_.c_str());
		return -1;
	}
	
	decodec_ctx_ = avcodec_alloc_context3(avodec);
	if (!decodec_ctx_) {
		av_log(NULL, AV_LOG_ERROR, "audio decode alloc decodec ctx failed.\n");
		return -1;
	}
	decodec_ctx_->sample_rate = sample_rate;
	decodec_ctx_->channel_layout = decode_ch_layout;
	if (0 != avcodec_open2(decodec_ctx_, avodec, NULL)) {
		av_log(NULL, AV_LOG_ERROR, "audio decode avcodec_open2 failed.\n");
		return -1;
	}
	ch_layout_ = decode_ch_layout;
	decode_format_ = decode_format;

	audioDecodeCreateFrame();
	
	return 0;
}

int AudioDecode::audioDecodePacket(AVPacket *srcPacket, AVFrame **dstFrame)
{
	int ret = avcodec_send_packet(decodec_ctx_, srcPacket);
	//ret >= 0说明数据设置成功了

	while (ret >= 0) {
		ret = avcodec_receive_frame(decodec_ctx_, frame_);
		if (ret < 0) {
			//说明没有帧了，要继续送数据
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				return EAGAIN;
			}
			else {
				return -1;//其他的值，说明真失败了。
			}
		}
		//获取到一个packet
		break;
	}
	*dstFrame = frame_;
	av_packet_unref(srcPacket);
	return 0;
}

int AudioDecode::audioDecodeCreateFrame()
{
	frame_ = av_frame_alloc();
	frame_->channel_layout = ch_layout_;
	frame_->format = decode_format_;
	frame_->nb_samples = 1024;
	int ret = av_frame_get_buffer(frame_, 0);
	if (ret != 0) {
		av_log(NULL, AV_LOG_ERROR, "open input failure.[%d]\n", AVERROR(ret));
		return -1;
	}
	printf("audio decode create frame size = %d\n", 
		1024 * av_get_bytes_per_sample(AV_SAMPLE_FMT_FLTP) * av_get_channel_layout_nb_channels(ch_layout_));
	return 0;
}
