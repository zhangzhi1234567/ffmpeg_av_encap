
#include <iostream>
#include <memory>
#ifdef _MSC_VER
#include <windows.h>
#include <tchar.h>
#include <dshow.h>
#include <atlcomcli.h>
#pragma comment(lib, "Strmiids.lib")
#endif
#include "audio_engine.h"
void	buffer_dump(char *buffer, int size) {
	printf("---------------------------------------------------------------------\n");
	printf("00000000 ---| ");
	for (int i = 0; i < size; i++) {
		printf("%02X ", (buffer[i] & 0xFF));
		if (!((i + 1) % 4)){
			printf("| ");
		}
		if (!((i + 1) % 16)){
			printf("\n%08X ---| ", ((i + 1) / 16) * 16);
		}
	}
	printf("\n");
	printf("---------------------------------------------------------------------\n");
}
void getAudioDevices(char* name)
{
#ifdef _MSC_VER
	CoInitialize(NULL);
	CComPtr<ICreateDevEnum> pCreateDevEnum;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pCreateDevEnum);
	CComPtr<IEnumMoniker> pEm;
	hr = pCreateDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEm, 0);
	if (hr != NOERROR) {
		return;
	}
	pEm->Reset();
	ULONG cFetched;
	IMoniker* pM = NULL;
	while (hr = pEm->Next(1, &pM, &cFetched), hr == S_OK)
	{
		IPropertyBag* pBag = 0;
		hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pBag);
		if (SUCCEEDED(hr))
		{
			VARIANT var;
			var.vt = VT_BSTR;
			hr = pBag->Read(L"FriendlyName", &var, NULL); //������������,��������Ϣ�ȵ�...
			if (hr == NOERROR)
			{
				//��ȡ�豸����
				WideCharToMultiByte(CP_ACP, 0, var.bstrVal, -1, name, 128, "", NULL);
				SysFreeString(var.bstrVal);
			}
			pBag->Release();
		}
		pM->Release();
	}

	pCreateDevEnum = NULL;
	pEm = NULL;
#else
	memcpy(name, "default", strlen("default") + 1);
#endif

}
#ifdef _MSC_VER
void convert(const char* strIn, char* strOut, int sourceCodepage, int targetCodepage)
{
	//LPCTSTR
	LPCTSTR pStr = (LPCTSTR)strIn;
	int len = lstrlen(pStr);
	int unicodeLen = MultiByteToWideChar(sourceCodepage, 0, strIn, -1, NULL, 0);
	wchar_t* pUnicode = NULL;
	pUnicode = new wchar_t[unicodeLen + 1];
	memset(pUnicode, 0, (unicodeLen + 1) * sizeof(wchar_t));
	MultiByteToWideChar(sourceCodepage, 0, strIn, -1, (LPWSTR)pUnicode, unicodeLen);

	BYTE* pTargetData = NULL;
	int targetLen = WideCharToMultiByte(targetCodepage, 0, (LPWSTR)pUnicode, -1, (char*)pTargetData, 0, NULL, NULL);

	pTargetData = new BYTE[targetLen + 1];
	memset(pTargetData, 0, targetLen + 1);
	WideCharToMultiByte(targetCodepage, 0, (LPWSTR)pUnicode, -1, (char*)pTargetData, targetLen, NULL, NULL);
	lstrcpy((LPSTR)strOut, (LPCSTR)pTargetData);

	delete pUnicode;
	delete pTargetData;
}
#endif

int ffmpeg_open_aac_file(char *file, AVFormatContext **fmt_ctx) {
	int ret = 0;
	ret = avformat_open_input(fmt_ctx, file, NULL, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "cant open file\n");
		return -1;
	}
	av_dump_format(*fmt_ctx, 0, "./encode.aac", 0);

	int result = avformat_find_stream_info(*fmt_ctx, NULL); // ������ļ��е�������Ϣ
	if (result < 0) {
		printf("fail avformat_find_stream_info result is %d\n", result);
		return -1;
	}
	else {
		printf("sucess avformat_find_stream_info result is %d\n", result);
	}
	int stream_index = av_find_best_stream(*fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0); // Ѱ����Ƶ���±�
	printf("stream_index is %d\n", stream_index);

	if (stream_index == -1) {
		printf("no audio stream\n");
		return -1;
	}

	return stream_index;
}

/*
** ���ԣ��������� AAC �ļ������ PCM�� ͨ�����ַ�ʽ��ȡ AAC
** 1��ͨ�� FFMPEG �ļ����װ��Ȼ�� av_read_frame ��ȡÿһ֡
** 2���� AAC �ļ�����һ�������� ���� ADTS ͷ���� DATA, ��� AVPacket
*/
void parse_aac_2_pcm() {
	int8_t aac_buffer[255];
	int size = 0;
	char *file_aac = "./encode.aac";
	char *file_pcm = "decode.pcm";
	

	FILE *fd_aac = fopen(file_aac, "rb+");
	FILE *fd_pcm = fopen(file_pcm, "wb+");
	string decoderName("aac");
	std::unique_ptr<AudioDecode> audioDecode(new AudioDecode(decoderName));
	audioDecode->audioDecodeInit(AV_CH_LAYOUT_STEREO, 44100, AV_SAMPLE_FMT_FLTP);

	AVPacket *packet = av_packet_alloc();
	av_init_packet(packet);
	AVFrame *decodeFrame = NULL;
	

	//�ز������
	AudioSample *audioSample = new AudioSample(44100, AV_SAMPLE_FMT_FLTP, AV_CH_LAYOUT_STEREO,
											   44100, AV_SAMPLE_FMT_FLT, AV_CH_LAYOUT_STEREO);
	audioSample->audioSampleInit();
	AVFrame *resample_frame = NULL;

	///��aac�ļ�����ffmpeg���ļ�����ͨ��av_read_frame ��ȡ packet
	AVFormatContext *fmtCtx = NULL;
	ffmpeg_open_aac_file(file_aac, &fmtCtx);
	while (0) {
		av_read_frame(fmtCtx, packet);
		printf("packet->stream_index = %d, packet.size = %d\n", packet->stream_index, packet->size);
		buffer_dump((char *)packet->data, packet->size);
		int ret = audioDecode->audioDecodePacket(packet, &decodeFrame);
		if (ret == EAGAIN)
			continue;
		//��������������� FLTP ��ʽ�ģ�ffplay ��֧�֣���Ҫ�ز���
		audioSample->audioSampleConvert(decodeFrame, &resample_frame);
		fwrite(resample_frame->data[0], 1, resample_frame->linesize[0], fd_pcm);
		printf("decode frame.size = %d\n", resample_frame->linesize[0]);
		fflush(fd_pcm);
	}

	//��aac�ļ����� aac ����������� AVPacket ����
	while (1) {
		memset(aac_buffer, 0, 255);
		fread(aac_buffer, 1, 7, fd_aac);
		size = 0;
		size |= (aac_buffer[3] & 0b00000011) << 11; //0x03  ǰ�������λ��Ҫ�Ƶ���λ��13 - 11 = 2��	
		size |= aac_buffer[4] << 3;                //�м��8bit,Ҫ�Ƶ�ǰ������λ��13 - 2 = 11 - 8 = 3
		size |= (aac_buffer[5] & 0b11100000) >> 5; //0xe0 ����3Bit��Ҫ�Ƶ����	
		
		fread(aac_buffer + 7, 1, size - 7, fd_aac); //size ������ adts ͷ���� adts ͷ���Ѿ������ˣ�����Ҫ��ȥ
		
		// ��װ AVPacket
		packet->size = size;
		packet->data = (uint8_t *)av_malloc(packet->size);
		memcpy(packet->data, aac_buffer, size);
		int ret = av_packet_from_data(packet, packet->data, packet->size);

		// ���� �� �ز��� 
		audioDecode->audioDecodePacket(packet, &decodeFrame);
		// ��������������� FLTP ��ʽ�ģ�ffplay ��֧�֣���Ҫ�ز���
		audioSample->audioSampleConvert(decodeFrame, &resample_frame);

		// ���� PCM ����
		fwrite(resample_frame->data[0], 1, resample_frame->linesize[0], fd_pcm);
		fflush(fd_pcm);
		printf("\n\n\n\n");
	}
}
/*
** ���ԣ�������ͷ�ɼ���Ƶ���ݣ��ɼ� --> �ز��� --> ���� --> ��� ADTS ͷ�� --> ���ļ�
** ֧��windows �� mac
** TODO: MAC ����һЩBUG
*/
int capture_encode_2_aac() {
	char device_name[128] = { 0 };
	char adts_buffer[7] = { 0 };
	AVFrame *frame = NULL;
	AVFrame *resample_frame = NULL;
	AVPacket *packet = NULL;
	char *file_name = "capture.pcm";
	FILE *fd = fopen(file_name, "wb+");

	char *file_name1 = "sample.pcm";
	FILE *fd_sample = fopen(file_name1, "wb+");

	char *file_name2 = "encode.aac";
	FILE *fd2 = fopen(file_name2, "wb+");
#ifdef _MSC_VER	
	char name[128] = { 0 };
	char name_utf8[128] = { 0 };
	getAudioDevices(name);
	convert(name, name_utf8, CP_ACP, CP_UTF8);
	sprintf(device_name, "audio=%s", name_utf8);
	printf("device_name:%s\n", device_name);
	string libName("dshow");
#elif __APPLE__
	device_name = ":0";
	string libName("avfoundation");
#endif

	string devName(device_name);
	AudioCapture *audioCapture = new AudioCapture(devName, libName);
	int ret = audioCapture->audioInit(AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 1024);
	if (ret < 0) {
		printf("init fail.\n");
		return 0;
	}
	AudioSample *audioSample = new AudioSample(44100, AV_SAMPLE_FMT_S16, AV_CH_LAYOUT_STEREO,
		44100, AV_SAMPLE_FMT_FLTP, AV_CH_LAYOUT_STEREO);
	audioSample->audioSampleInit();

	string encoderName("aac");
	AudioEncode *audioEncode = new AudioEncode(encoderName);
	ret = audioEncode->audioEncodeInit(AV_SAMPLE_FMT_FLTP, AV_CH_LAYOUT_STEREO, 44100, 16000, FF_PROFILE_AAC_LOW);
	if (ret != 0) {
		printf("audio encode init fail.\n");
		return 0;
	}
	while (1) {
		ret = audioCapture->audioCaptureFrame(&frame);
		if (ret < 0) {
			if (ret == -35)
				continue;
			exit(0);
		}
		fwrite(frame->data[0], 1, frame->linesize[0], fd);

		printf("frame linesize size = %d\n", frame->linesize[0]);
		audioSample->audioSampleConvert(frame, &resample_frame);
		//�ز����������� planar ģʽ AV_SAMPLE_FMT_FLTP
		fwrite(resample_frame->data[0], 1, resample_frame->linesize[0], fd_sample);
		fwrite(resample_frame->data[1], 1, resample_frame->linesize[0], fd_sample);
		printf("sample frame linesize size = %d\n", resample_frame->linesize[0]);
		fflush(fd);
		fflush(fd_sample);

		ret = audioEncode->audioEncode(resample_frame, &packet);
		if (ret == EAGAIN)
			continue;
		if (ret == -1)
			break;
		printf("encode packet size = %d\n", packet->size);
		audioEncode->packetAddHeader(adts_buffer, packet->size);
		fwrite(adts_buffer, 1, 7, fd2);
		fwrite(packet->data, 1, packet->size, fd2);
		fflush(fd2);
	}
	audioEncode->audioEncode(NULL, &packet); //����֮��Ҫ��һ�������ݣ��ñ������³���������ݡ�
	return 0;
}

/*
** ����������
*/
int main() {
	
#if 1
	//capture_encode_2_aac();
	
	parse_aac_2_pcm();
#endif
}


