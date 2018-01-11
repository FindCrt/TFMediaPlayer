1. 找不到FFmpeg的函数符号，因为在c++文件里导入，使用c++的方式链接，而FFmpeg是c函数符号。使用`extern "C"`

2. 编译优化：
 * [一个参考](https://gist.github.com/lulalala/7028049)
 * `Invalid data found when processing input`缺少了demuxer
 * `av_strerror `解析错误

3. 音频播放：
 * 音频和视频的驱动点不同，流动方式不同。视频是数据源**主动**的推送数据，并且是离散的一帧帧明确分开的，而音频是系统播放器主动拉取数据，也就是数据源**被动**输出数据，并且音频是连续的，传递给系统的时间，并不是这一帧的播放时间，它更像是一个**时间段**，而不是点。
 * 格式信息在stream的codecpar(AVCodecParameters类型)里，不需要等得到AVFrame的时候再去获取格式信息。
 * 问题：音频在文件或传输中都是编码后的类型，是什么决定它解码后是什么类型？是在原数据中就携带了，还是在解码过程中可选的呢？
 * 格式转换添加planar，也就是声道数据分开。audioUnit的填充audio buffer的方法也要稍微修改，传入uint8_t**,即指针的指针，以及planar数量。
 * 构建audioUnit的音频格式，使用系统提供的`FillOutASBDForLPCM`更方便
 * 音频的解码是一个packet对应多个frame.
 * 音频的linesize只要第一个有效，因为每个linesize强制一样。
 * FFmpeg的linesize单位是byte.
 * 奇怪的是`AV_SAMPLE_FMT_FLTP`格式，linesize为8192,nb_samples为1024，算起来bitPerSample为8.
 * `av_frame_clone`返回为null

4. 音视频同步
 * 使用remainTime，如果计算失误，会导致sleep非常长时间。
 
 
5. 缓冲区
 * 一个隐患是不同流使用的是同一个获取线程，如果某个流满了，难么所有流的获取都要停滞。比如音频packet缓冲区满了，但是视频缓冲区还缺少，这时不解析packet了，视频获取不了，播不下去，那么音频就不会减少，然后又返回来继续阻塞，然后整个流程就死了。
 * 核心问题是音频packet和视频packet的数量对比，还有frame。他们可能不是1：1的。

6. 音频的重采样
 * 