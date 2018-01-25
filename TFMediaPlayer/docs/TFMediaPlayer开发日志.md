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
 * `av_frame_clone`返回为null，如果src不是ref的话。
 

4. 音视频同步
 * 使用remainTime，如果计算失误，会导致sleep非常长时间。
 * 如果音频消耗慢，视频消耗快，那么音频会越来越多，直到音频缓冲区满了，这时如果解析到一个音频packet，那么就会一直等待音频缓冲区有空间。而视频那边还在不断播放，直到视频空了。可能出现奇怪问题。**但是正常解析应该不会出现这样的情况**
 * 针对上述问题：1.具体怎么死锁？ 2.是否动态扩展缓冲区的limit来避免此类问题？
 * 并没有死锁，是播放器停止了，但是没停止音频播放器。上述情况应该可以不处理
 
 
5. 缓冲区
 * 一个隐患是不同流使用的是同一个获取线程，如果某个流满了，难么所有流的获取都要停滞。比如音频packet缓冲区满了，但是视频缓冲区还缺少，这时不解析packet了，视频获取不了，播不下去，那么音频就不会减少，然后又返回来继续阻塞，然后整个流程就死了。
 * 核心问题是音频packet和视频packet的数量对比，还有frame。他们可能不是1：1的。

6. 音频的重采样
 * resample的参数是channel_layout，而不是channels，修改channels而没有修改channel_layout，导致不匹配错误。
 * `swr_convert`的输入和输出都是针对perChannel的

 
7. 哒哒声的问题：
  >今天刚好碰到同样的问题，google, baidu均无解，无奈自己研究，是因为语音数据里包含了RTP包头的原因，自己写了个转换工具，把12Bytes的RTP包头去掉，哒哒声就没了
  
  * 哒哒声是因为突然的数值变大，导致音爆。很可能是数据读取位置错误，导致读数错误。而且每次解码同一个音频的数据也不同，很奇怪
  * 第一步： 修改swr resample的方法
  * nb_samples的计算有很多值得深究，`swr_get_delay`是干嘛的？`swr_get_out_samples`有没有用？`nb_samples`到底是不是单个channel的？
  * resample修改后，数据变稳定了，虽然还是坏的。稳定原因可能是nb_samples的问题，之前可能是不固定的，待查。
  * 第二步：使用s16的纯pcm音频来播放，仍然存在问题。排除是resample的问题。
  * 查看声波图，数据稳定、分散、空白较多。听起来，速度加快了，但原节奏基本在。速度加快这一条可以先查。
  * 使用`av_fast_malloc`和一个固定的内存来保留读取时遗留的一段buffer.`av_fast_malloc`第二个参数在成功是可能不等于第三个参数，有效的遗留buffer长度不能根据第二个参数的返回值来。
  * s16单声道pcm播放没问题了，问题出在读取音频buffer的时候，读了buffer,而不是buffer[i]。**内存相关处理里，这个点是非常危险的，而且是很容易出错的，把指针的指针传给指针，编译器是区分不出来的**
  * 还需要检查之前的代码是否也是这个问题，还有AAC播放是否可以了。
  * 带视频的双声道音频AAC还是杂音，纯音频AAC可以了，第一个sampleRate是48k,第二个是44.1k。码率不同，而且不是倍数，很可能造成错位问题。
  * 11.025k的MP3格式播放基本没问题，高音处会有杂音，有点像数值溢出。
  * game视频播放已经没有音爆了，说明之前的代码也是有上述问题，就是buffer读错了。现在的问题是变音了，像扭曲了一样，但是真个的节奏都在，声音没丢。` memcpy(buffer, dataBuffer, needReadSize);`错误就是这句，一直在。
  * 重大突破：使用audioQueue，把sampleRate设为和音频源一致，然后每次的size也和resample之后每帧一致，一点杂音都没有。**resample和读取可能都有问题，错位的感觉**
 * 把queueBuffer的samples调整到2048,出现后半截就是空值。
 * **问题找到了，跟resample没关系，还是我代码写错了，拷贝buffer的时候，没有考虑到可能已经拷贝了一段内存，所以后面的会覆盖前面的内存，导致后面的内存空掉**
 * **有价值的是：那些莎莎声，都是丢失的数据，也就是空白**这也和为什么电脑卡顿之类的情况时，会有莎莎声。运行不流畅，数据没有接上，导致数据空白。


8. 视频播放
 * 图像出现错位、扭曲，这个不好描述，有点像衣服扣子系错了的感觉。就是层和层之间的错位。[有可能是这里描述的问题](http://blog.csdn.net/grafx/article/details/29185147)
 * 宽度是568，但linesize是576.这个两个值不同，有8个字节是无用的。
 * 如果把播放宽度设为linesize,那么播放画面正常，但是结尾是绿色，就是没有数据。
 * 每个plane的数据是连续的，但是显示到界面上是2维的，也就是要切割成n段，每段是屏幕上的一行。而linesize就是每一段的长度，影响着从哪里切开的问题。上面的问题就是使用width当做linesize,而width<linesize,导致切割的地方短了，剩余的部分流到了下一层，把下一层往后挤，导致层层错位。
 * **构建texture同时需要width和linesize,width决定哪些数据时有效的，linesize决定每一行的数据怎么划分**
 * [texture对齐和裁剪](http://www.zwqxin.com/archives/opengl/opengl-api-memorandum-2.html)
 * 使用`glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[0]);`来形成裁剪后的图像，避开结尾无用的数据。
 * 底部还是有绿色，说明还是有空数据，高度有问题？texture的高度是240

 
之前调试一致是用的本地文件，没想到网略音频一下就可以播通了，高兴！