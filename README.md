# ffmpeg_av_encap
封装了 ffmpeg 接口，实现音频 视频的采集，重采样，编码， 解码等

## 音频实现以下几个类：
### AudioCapture
音频采集类，需要提供采集的设备名和底层库(windows: dshow ; mac: avfoundation);
first call audioInit() init Audio param and open device, then call audioCaptureFrame(), get a frame pcm data.

### AudioSample
音频重采样类
first call audioSampleInit() init Audio param and open device, then call audioSampleConvert(), convert a frame.

### AudioEncode
音频编码类 输入frame 输出packet
first call audioEncodeInit() init Audio encode param and open encoder, then call audioEncode(), get a packet data.
if you want get aac data, you kan call packetAddHeader() add adts header.
