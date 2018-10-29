重读ijk之后，有一些值得改进的东西，最主要的是serial的引用和同步钟的设计。

1. 同步的逻辑和ijk不一样，但是结果证明是一样的，而且我的更加简洁。
 * 避免后期问题，还是采用多个钟的方式，但计算使用之前方式。
 * 同步钟记录时精度问题也会造成数据丢失，注意`AV_TIME_BASE`是int

2. 获取当前媒体时间：seeking在一开始就重置了，距离正确开始播放还有一段时间，这之间怎么防止犯错的？出错就会产生进度条闪回的效果。
3. 内容不播的时候，时间还是往后走，因为时间是预测的、跟现实时间关联的，有漏洞。
4. `decoder_decode_frame`的代码还是有漏洞的，只是可能性比较小，如果serial的修改在`if (d->queue->serial == d->pkt_serial) {`这个判断之后，并且之后取frame一路顺畅，那么这个错误的frame会一直传入到frameQueue里面并且serial是新的。之所以没有出错，是在视频播放时`last_duration = vp_duration(is, lastvp, vp);`的计算里，使用了`if (isnan(duration) || duration <= 0 || duration > is->max_frame_duration)`的判断，返回seek的时候满足<=0,往前seek的时候满足`duration > is->max_frame_duration`。这时取得值是frame自身的duration，根据帧率计算的一个值，然后播放之后就一些重置了。
5. video_refresh里使用duration的好处是只跟上一帧比较，而不是直接的和同步钟的时间比较。
6. 第4点是错的`decoder_decode_frame `这个是没有漏洞的，因为读取到frame之后是break而不是return,还是要经过第二部分的serial检测。除非是已经返回的frame,这种概率就非常小了。再加上有duration把控，不会出错了。