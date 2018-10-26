###流程

#####VTB解码的要素

几个核心角色：

* `VTDecompressionSessionRef` 管理解码的session，源文件的格式和输出格式在这里指定
* `CMSampleBufferRef` 这个对应h264里的NALU，原始视频流要封装成这个，才可传给VTB解码。

流程分成两大部分：构建解码的session和解码。

**构建session：**

 先构建`CMFormatDescriptionRef`,这个是重点。有两种构建方法:`CMVideoFormatDescriptionCreate`和`CMVideoFormatDescriptionCreateFromH264ParameterSets`。前一种是使用属性列表来构建，指定一系列的属性设置；后一种是使用h264的*sps*和*pps*来构建。
 
 sps和pps是包含在h264的视频流里的，是一种特殊的NALU,type分别是7和8。
 
 构建完formatDesc之后，使用`VTDecompressionSessionCreate`构建session,除了formatDesc之外，还有两个重要的东西：
 
 * `destinationImageBufferAttributes`,目标图像的属性，如宽高、颜色空间等
 * `outputCallback` 输出的回调函数，因为解码可以是异步的，但在输出的回调函数里一定是可以抓到输入的图像的。所以解码的最后一站就是在这个回调函数里，在这拿到解码后的图像。

 
**解码视频：**

首先要获取到视频的数据，两种方法：第一个就是使用ffmpeg，拿到AVPacket就是了；另一种就是自己解析，这就需要 1.从容器里分离出视频流 2.使用startCode分离出NALU。

后一种的麻烦不在于分离出NALU,而是面对许多不同的容器(mp4 flv等)要分离出视频流，必定要熟知它们，学习曲线有点陡，所以只能弃用。

如果是调试时，还一种选择，就是只有视频流的h264文件。这样就略去了demux的操作了。

拿到AVPacket之后，会有多种情况，因为h264的NALU有两种不同的格式：

 * 开头3或4个字节的数值是后面数据的长度
 * 开头是0x001或0x0001,也就是只是一个标识。

 拿到AVPacket之后，每种情况都有可能，而VTB需要的是**[4字节长度值][数据]**这种格式，所以还需要转一下。
 
 有了数据，就开始解码：
 
 * 使用`CMBlockBufferCreateWithMemoryBlock`构建`CMBlockBufferRef`，这个其实也还只是一段内存，没什么特别
 * 使用`CMSampleBufferCreate`构建`CMSampleBufferRef`这个提供了formatDesc，所以应该包含了对视频数据的解析了。
 * `VTDecompressionSessionDecodeFrame`提供session和前面的`sample`解码视频。这个最核心的一步，前面所有的操作都是为了这一步准备。
 * 在`outputCallback `里，有个参数`imageBuffer`这个就是解码后的视频数据了。

#####和ffmpeg结合的问题

因为自己实现demux太难了，所以采用ffmpeg,正好也可以使用ffmpeg的其他功能。

使用这个之后，就不用自己解析NALU了，拿到的就直接是AVPacket,但同时也拿不到sps和pps了(还需严格考证)。

构建session的方法变了。参考ijkplayer，依赖了ffmpeg的codecParamters。需要参考ffmpeg本身对h264的解码的代码才能知道是否有方法可以直接拿到pps和sps。

###值得注意的bug

1. 细小精度偏差带来的大问题

 `(5/4)*ysize` 改为 `ysize/4*5`就显示正常了，猜测是对的。5/4会引入小数，导致结果偏移，ysize是10万界别的大数，一点点的精度偏移最后可能会导致1个数的偏差，从而导致v分量的取值位置偏移，所有v分量的取值全部出错。
 
2. 硬解码的pixelBuffer的释放
 * pixelBuffer的内存用`CVPixelBufferRetain`和`CVPixelBufferRelease`来控制，逻辑应该还是引用只是那一套。
 * nv12不能直接播放，转到yuv420会复制内存，这部分内存需要释放

3. 解码器的函数重名inline函数，最后都会定位到同一个函数，并不会保留不同的版本。函数定义成decoder的静态成员函数，带上decoder的命名空间，不会导致重名。
4. 帧间时间跟播放时间的问题，如果播放时间小于帧间时间，就会不断往前调解，remain超出0.01后，又调回准确时间播放，是一个良性的循环。但如果播放时间大于帧间时间，就会不断后台，播放时间越来越晚，直到超出0.01，然后丢弃这一帧，再次回到准确时间播。但是后者情况会导致一些帧被丢弃，所以要尽量的减少播放时间。所以把颜色空间转换(nv12转yuv420)的操作放在播放之前，也即是先转换再入frame buffer.
5. frame的pts可能并不是按照pts排序好的，直接播放会导致丢帧错乱。ffmpeg解码后的frame是按照pts拍好序的，而vtb没有做这个操作。对frame缓冲区进行排序。


###问题

1. 完全不知道`SampleDescriptionExtensionAtoms`这个东西是从哪里得来的，Apple的文档里根本没有，难道是先编码，看编码后的`CMVideoFormatDescriptionRef`是什么样的，然后反过来知道解码该怎么设置？很有可能
 
 ```
 CMFormatDescriptionRef fmtDesc = CMSampleBufferGetFormatDescription(sampleBuffer);
    CFDictionaryRef extensions = CMFormatDescriptionGetExtensions(fmtDesc);
 ```
2. `avcC`的格式是明确的，可以从里面拿到sps跟pps,这样其实也就可以用另外的函数构建formatDesc了。
3. 使用`GL_LUMINANCE_ALPHA`可以构建双通道的texture,但是对于某些视频会错乱，还需要再查。