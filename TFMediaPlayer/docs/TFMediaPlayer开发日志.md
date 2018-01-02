1. 找不到FFmpeg的函数符号，因为在c++文件里导入，使用c++的方式链接，而FFmpeg是c函数符号。使用`extern "C"`

2. 编译优化：
 * [一个参考](https://gist.github.com/lulalala/7028049)
 * `Invalid data found when processing input`缺少了demuxer
 * `av_strerror `解析错误