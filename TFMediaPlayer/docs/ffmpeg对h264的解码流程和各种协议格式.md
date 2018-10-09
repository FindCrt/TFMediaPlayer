1. [从mp4中提取h264的sps和pps](https://www.cnblogs.com/skyseraph/archive/2012/04/01/2429384.html)
2. [核心的stbl box](https://blog.csdn.net/datamining2005/article/details/72820195?locationNum=15&fps=1)

 >“stbl”包含了关于track中sample所有时间和位置的信息，以及sample的编解码等信息。利用这个表，可以解释sample的时序、类型、大小以及在各自存储容器中的位置。
 
3. stsd是其中首要的box,视频的编码类型、宽高、长度，音频的声道、采样等信息都会出现在这个box中。然后编码器的box是`avc1`,再到里面的`avcC`,pps和sps就在这里。

 ![](/Users/apple/Documents/avcC的结构.png)
4. `avformat_open_input`->`s->iformat->read_header(s)`，是MP4文件，到`mov_read_header`,在这里按照mp4的嵌套box的格式，解析视频的信息。读取box的函数是`mov_read_default`,这个是会不断嵌套调用的。`mov_default_parse_table`包含了box的标识和对应的解析函数的映射表，解析某个box的时候，不断检测标识，发现了新的box,就从这个映射表里拿到解析函数，然后解析。
5. 但是flv的文件，视频信息并不是放在头里面的，而是在h264的数据里，应该是按照原格式存放的。从`avformat_find_stream_info`开始，到`read_frame_internal`把视频的packet读取出来解析，猜得到了视频的信息。
6. [ffmpeg里SPS结构体的定义](http://www.ffmpeg.org/doxygen/2.1/structSPS.html) 高宽都在sps里面，
  [sps的原始结构](https://www.cnblogs.com/wainiwann/p/7477794.html)，前面那个是ffmpeg里的结构体，它是重定义的模型，配合`ff_h264_decode_seq_parameter_set`才知道意义。
  ![sps的原始结构图](/Users/apple/Downloads/sps结构.jpg)
7. [flv对h264的封装](https://blog.csdn.net/xin_hua_3/article/details/45481843) 
8. [更清晰的flv说明](https://www.cnblogs.com/chgaowei/p/5445597.html) flv的sps和pps的存在的部分，也是按照`AVCDecoderConfigurationRecord `这个结构来的，就是mp4里avcC这个box的结构。
9. sps的原始格式解析表中`ue(v)`表示无指数哥伦布熵编码，具体参考[H264 指数哥伦布编码](https://blog.csdn.net/szfhy/article/details/80901974)
10. ffmpeg对sps的解析，为什么跳过了第一个字节？这个很奇怪`ff_h264_decode_seq_parameter_set`方法一开始gb的index就是8。

11. 一个avcC的例子：

```
01   //版本
64001f
ff   //后3位为3，3+1=4为nalu长度

e1  //后5位为1，sps的个数
001d	//sps的数据长度，这个头固定2字节，值为29

//奇怪的就在这里，ffmpeg对sps的解析，从0x64那开始，读取的profile_idc为100，即0x64的十进制。
6764001f acd9c0f0 116ffc02 20021c40 00000300 4000000c 03c60c67 80

01   //pps的个数，为1
0004  //pps数据长度，为4
68e9bbcb
```