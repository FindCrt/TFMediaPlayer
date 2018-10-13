###带问题阅读

1. 主流程上的区别
2. 缓冲区的设计
3. 内存管理的逻辑
4. 音视频播放方式
5. 音视频同步
6. seek的问题：缓冲区flush、播放时间显示、k帧间距大时定位不准问题...
7. stop时怎么释放资源，是否切换到副线程？
8. 网络不好时的处理，如获取frame速度慢于消耗速度时，如果不暂停，会一致卡顿，是否会主动暂停？
9. VTB的解码和ffmpeg的解码怎么统一的？架构上怎么设计的？



####数据流向

#####音频

* `av_read_frame`
* `packet_queue_put`
* `audio_thread`+`decoder_decode_frame`+`packet_queue_get_or_buffering`
* `frame_queue_peek_writable`+`frame_queue_push`
* `audio_decode_frame`+`frame_queue_peek_readable`,数据到`is->audio_buf`
* `sdl_audio_callback`，数据导入到参数stream里。这个函数是上层的音频播放库的buffer填充函数，如iOS里使用audioQueue,回调函数`IJKSDLAudioQueueOuptutCallback`调用到这里，然后把数据传入到audioQueue.

#####视频

读取packet部分一样

* `video_thread`，然后`ffpipenode_run_sync`里硬解码定位到`videotoolbox_video_thread`,然后`ffp_packet_queue_get_or_buffering`读取。
* `VTDecoderCallback`解码完成回调里，`SortQueuePush(ctx, newFrame);`把解码后的pixelBuffer装入到一个有序的队列里。
* `GetVTBPicture`从有序队列里把frame的封装拿出来，也就是这个有序队列只是一个临时的用来排序的工具罢了，**这个思想是可以吸收的**；`queue_picture`里，把解码的frame放入frame缓冲区
* 显示`video_refresh`+`video_image_display2`+`[IJKSDLGLView display:]`
* 最后的纹理生成放在了render里，对vtb的pixelBuffer,在`yuv420sp_vtb_uploadTexture`。使用render这个角色，渲染的部分都抽象出来了。shader在`IJK_GLES2_getFragmentShader_yuv420sp`


**结论：主流程上没有大的差别。**


#####缓冲区的设计

**packetQueue:**

1. 数据机构设计

 packetQueue采用两条链表，一个是保存数据的链表，一个是复用节点链表，保存没有数据的那些节点。数据链表从`first_pkt`到`last_pkt`,插入数据接到`last_pkt`的后面，取数据从`first_pkt `拿。复用链表的开头是recycle_pkt，取完数据后的空节点，放到空链表recycle_pkt的头部，然后这个空节点成为新的recycle_pkt。存数据时，也从recycle_pkt复用一个节点。
 
 链表的节点像是包装盒，装载数据的时候放到数据链表，数据取出后回归到复用链表。
 
2. 进出的阻塞控制

 取数据的时候可能没有，那么就有几种处理：直接返回、阻塞等待。它这里的处理是阻塞等待，并且会把视频播放暂停。所以这个回答了**问题8**，外面看到的效果就是：网络卡的时候，会停止播放然后流畅的播放一会，然后又继续卡顿，播放和卡顿是清晰分隔的。
 
 进数据的时候并没有做阻塞控制，为什么数据不会无限扩大?
 
 是有阻塞的，但阻塞不是在packetQueue里面，而是在readFrame函数里：
 
 ```
 if (ffp->infinite_buffer<1 && !is->seek_req &&
              (is->audioq.size + is->videoq.size + is->subtitleq.size > ffp->dcc.max_buffer_size
            || (   stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq, MIN_FRAMES)
                && stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq, MIN_FRAMES)
                && stream_has_enough_packets(is->subtitle_st, is->subtitle_stream, &is->subtitleq, MIN_FRAMES)))) {
            if (!is->eof) {
                ffp_toggle_buffering(ffp, 0);
            }
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
 ```
 
 简化来看就是：
 
 * `infinite_buffer `不是无限的缓冲
 * `is->audioq.size + is->videoq.size + is->subtitleq.size > ffp->dcc.max_buffer_size`,使用数据大小做限制
 * `stream_has_enough_packets `使用数据的个数做限制

 因为个数设置到了50000，一般达不到，而是数据大小做了限制，在15M左右。
 
 这里精髓的地方有两点：
 
 * 采用了数据大小做限制，因为对于不同的视频，分辨率的问题会导致同一个packet差距巨大，而我们实际关心的其实就是内存问题。
 * 暂停10ms，而不是无限暂停等待条件锁的signal。从设计上说会更简单，而且可以避免频繁的wait+signal。这个问题还需仔细思考，但直觉上觉得这样的操作非常好。


**frameQueue:**

 数据使用一个简单的数组保存，可以把这个数据看成是环形的，然后也是其中一段有数据，另一段没有数据。`rindex`表示数据开头的index,也是读取数据的index,即read index,`windex`表示空数据开头的index,是写入数据的index,即write index。
 
 也是不断循环重用，然后size表示当前数据大小，max_size表示最大的槽位数，写入的时候如果size满了，就会阻塞等待；读取的时候size为空，也会阻塞等待。
 
 有个奇怪的东西是`rindex_shown`,读取的时候不是读的`rindex`位置的数据，而是`rindex+rindex_shown`,需要结合后面的使用情况再看这个的作用。后面再看。
 
**还有serial没有明白什么意思**
 

**结论：**缓冲区的设计和我的完全不同，但都使用**重用**的概念，而且节点都是包装盒，数据包装在节点里面。性能上不好比较，但我的设计更完善，frame和packet使用统一设计，还包含了排序功能。


#####内存管理

1. packet的管理
 
 * 从`av_read_frame`得到初始值，这个时候引用数为1，packet是使用一个临时变量去接的，也就是栈内存。
 * 然后加入队列时，`pkt1->pkt = *pkt;`使用值拷贝的方式把packet存入，这样缓冲区的数据和外面的临时变量就分离了。
 * `packet_queue_get_or_buffering`把packet取出来，同样使用值复制的方式。
 * 最后使用`av_packet_unref`把packet关联的buf释放掉，而临时变量的packet可以继续使用。
 
 需要注意的一点是:`avcodec_send_packet`返回`EAGAIN`表示当前还无法接受新的packet，还有frame没有取出来，所以有了：
 
 ```
 d->packet_pending = 1;
 av_packet_move_ref(&d->pkt, &pkt);
 ```
 把这个packet存到d->pkt，在下一个循环里，先取frame，再把packet接回来，接着上面的操作：
 
 ```
 if (d->packet_pending) {
     av_packet_move_ref(&pkt, &d->pkt);
     d->packet_pending = 0;
 }
 ```
 
 可能是存在B帧的时候会这样，因为B帧需要依赖后面的帧，所以不会解码出来，等到后面的帧传入后，就会有多个帧需要读取。这时解码器应该就不接受新的packet。但ijkplayer这里的代码似乎不会出现这样的情况，因为读取frame不是一次一个，而是一次性读到报`EAGAIN`错误未知。待考察。
 
 另，`av_packet_move_ref`这个函数就是完全的只复制，source的值完全的搬到destination,并且把source重置掉。其实就是搬了个位置，buf的引用数不改变。
 
 
2. 视频frame的内存管理

 * 在`ffplay_video_thread`里，frame是一个对内存，使用`get_video_frame`从解码器读取到frame。这时frame的引用为1
 * 过程中出错，使用`av_frame_unref`释放frame的buf的内存,但frame本身还可以继续使用。不出错，也会调用`av_frame_unref`，这样保证每个读取的frame都会unref,这个unref跟初始化是对应的。**使用引用指数来管理内存，重要的原则就是一一对应。**
 
 因为这里只是拿到frame，然后存入缓冲区，还没有到使用的时候，如果buf被释放了，那么到播放的时候，数据就丢失了,所以是怎么处理的呢？
 
 存入缓冲区在`queue_picture`里，再到`SDL_VoutFillFrameYUVOverlay`,这个函数会到上层，根据解码器不同做不同处理，以`ijksdl_vout_overlay_ffmpeg.c`的`func_fill_frame`为例。
 
 有两种处理：
 
 * 一种是overlay和frame共享内存，就显示的直接使用frame的内存，格式是YUV420p的就是这样，因为OpenGL可以直接显示这种颜色空间的图像。这种就只需要对frame加一个引用，保证不会被释放掉就好了。关键就是这句：`av_frame_ref(opaque->linked_frame, frame);`
 * 另一种是不共享，因为要转格式，另建一个frame，即这里的`opaque->managed_frame`,然后转格式。数据到了新地方，原frame也就没用了。不做ref操作，它自然的就会释放了。
 
3. 音频frame的处理

 在`audio_thread`里，不断通过`decoder_decode_frame`获取到新的frame。和视频一样，这里的frame也是对内存，读到解码后的frame后，引用为1。音频的格式转换放在了播放阶段，所以这里只是单纯的把frame存入：`av_frame_move_ref(af->frame, frame);`。做了一个复制，把读取的frame搬运到缓冲区里了。

在frame的缓冲区取数据的时候，`frame_queue_next`里包含了`av_frame_unref`把frame释放。**这个视频也是一样**。

有一个问题是，上层播放器的读取音频数据的时候，frame必须是活的，因为如果音频不转换格式，是直接读取了frame里的数据。所以也就是需要在填充播放器数据结束后，才可以释放frame。

unref是在`frame_queue_next`，而这个函数是在下一次读取frame的时候才发生，下一次读取frame又是在当前的数据读完后，所以读完了数据后，才会释放frame,这样就没错了。

```
//数据读完才会去拉取下一个frame
if (is->audio_buf_index >= is->audio_buf_size) {
    audio_size = audio_decode_frame(ffp);
```


#####音视频的播放方式

音频播放使用AudioQueue:

* 构建AudioQueue：`AudioQueueNewOutput`
* 开始`AudioQueueStart`,暂停`AudioQueuePause`,结束`AudioQueueStop`
* 在回调函数`IJKSDLAudioQueueOuptutCallback`里，调用下层的填充函数来填充AudioQueue的buffer。
* 使用`AudioQueueEnqueueBuffer`把装配完的AudioQueue Buffer入队，进入播放。

上面这些都是AudioQueue的标准操作，特别的是构建`AudioStreamBasicDescription`的时候，也就是指定音频播放的格式。格式是由音频源的格式决定的，在`IJKSDLGetAudioStreamBasicDescriptionFromSpec`里看，除了格式固定为pcm之外，其他的都是从底层给的格式复制过来。这样就有了很大的自由，音频源只需要解码成pcm就可以了。

而底层的格式是在`audio_open`里决定的，逻辑是：

* 根据源文件，构建一个期望的格式`wanted_spec`,然后把这个期望的格式提供给上层，最后把上层的实际格式拿到作为结果返回。**一个类似沟通的操作，这种思维很值得借鉴**
* 如果上传不接受这种格式，返回错误，底层修改channel数、采样率然后再继续沟通。 
* 但是样本格式是固定为s16,即signed integer 16,有符号的int类型，位深为16比特的格式。位深指每个样本存储的内存大小，16个比特，加上有符号，所以范围是[-2^15, 2^15-1],2^15为32768，变化性足够了。

因为都是pcm,是不压缩的音频，所以决定性的因素就只有：采样率、通道数和样本格式。样本格式固定s16，和上层沟通就是决定采样率和通道数。

这里是一个很好的分层架构的例子，底层通用，上层根据平台各有不同。


视频的播放：

播放都是使用OpenGL ES，使用`IJKSDLGLView`,重写了`layerClass`,把layer类型修改为`CAEAGLLayer`可以显示OpenGL ES的渲染内容。所有类型的画面都使用这个显示，有区别的地方都抽象到`Render`这个角色里了，相关的方法有：

 * `setupRenderer` 构建一个render
 * `IJK_GLES2_Renderer_renderOverlay` 绘制overlay。

render的构建包括：
 
 * 使用不同的fragmnt shader和共通的vertex shader构建program
 * 提供mvp矩阵
 * 设置顶点和纹理坐标数据
 
render的绘制包括：

 * `func_uploadTexture`定位到不同的render,执行不同的纹理上传操作
 * 绘制图形使用`glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); `,使用了图元GL_TRIANGLE_STRIP而不是GL_TRIANGLE，可以节省顶点。

提供纹理的方法也是重点，区别在于颜色空间以及元素的排列方式：
 
 * rgb类型的提供了3种：565、888和8888。rgb类型的元素都是混合在一起的，也就是只有一个层(plane)，565指是rgb3个元素分别占用的比特位数，同理888，8888是另外包含了alpha元素。所以每个像素565占2个字节，888占3个字节，8888占4个字节。

 ```
 glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     widths[plane],
                     heights[plane],
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     pixels[plane]);
 ```
 构建纹理的时候区别就在format跟type参数上。
 
 * yuv420p的，这种指的是最常用的y、u、v3个元素全部开，分3层，然后数量比是4:1:1，所以u v的纹理大小高和宽都是y纹理的一半。然后因为每个分量各自一个纹理，所以每个纹理都是单通道的，使用的format为`GL_LUMINANCE`
 * yuv420sp的，这种yuv的比例也是4:1:1，区别在于u v不是分开两层，而是混合在同一层里，分层是uuuuvvvv,混合是uvuvuvuv。所以构建两个纹理，y的纹理不变，uv的纹理使用双通道的格式`GL_RG_EXT`,大小也是y的1/4（高宽都为1/2）。这种在fragment shader里取值的时候会有区别:
 
 ```
 //3层的
 yuv.y = (texture2D(us2_SamplerY, vv2_Texcoord).r - 0.5);
        yuv.z = (texture2D(us2_SamplerZ, vv2_Texcoord).r - 0.5);
 //双层的
 yuv.yz = (texture2D(us2_SamplerY,  vv2_Texcoord).rg - vec2(0.5, 0.5));
 ```
 uv在同一个纹理里，texture2D直接取了rg两个分量。
 
* yuv444p的不是很懂，看fragment shader貌似每个像素有两个版本的yuv，然后做了一个插值。
* 最后是yuv420p_vtb,这个是VideoToolBox硬解出来的数据的显示，因为数据存储在CVPixelBuffer里，所以直接使用了iOS系统的纹理构建方法。


ijkplayer里的的OpenGL ES是2.0版本，如果使用3.0版本，双通道可以使用`GL_LUMINANCE_ALPHA`。


#####音视频同步

首先看音频，音频并没有做阻塞控制，上层的的播放器要需要数据都会填充，没有看到时间不到不做填充的操作。所以应该是默认了音频钟做主控制，所以音频没做处理。

视频的控制在`video_refresh`里，播放函数是`video_display2`,进入这里代表时间到了、该播了，这是一个检测点。

有几个参数需要了解：

* `is->frame_timer`,这个时间代表上一帧播放的时间
* `delay`表示这一帧到下一帧的时间差


```
if (isnan(is->frame_timer) || time < is->frame_timer){
    is->frame_timer = time;
}
```
上一帧的播放时间在当前时间后面，说明数据错误，调整到当期时间

```
if (time < is->frame_timer + delay) {
    *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
    goto display;
}
```
`is->frame_timer + delay`就表示当前帧播放的时间，这个时间晚于当前时间，就表示还没到播放的时候。

**这个有个坑**：`goto display`并不是去播放了，因为display代码块里还有一个判断，判断里有个`is->force_refresh`。这个值默认是false,所以直接跳去display，实际的意义是啥也不干，结束这次判断。

反之，如果播放时间早于当前时间，那就要马上播放了。所以更新上一帧的播放时间：`is->frame_timer += delay;`。

然后一直到后面,有个`is->force_refresh = 1;`,这时才是真的播放。

从上面两段就可以看出基本的流程了：

一开始当前帧播放时间没到，goto display等待下次循环，然后时间不段后移，终于播放时间到了,播放当前帧，frame_timer更新为当前帧的时间。然后又重复上面的过程，去播放下一帧。

然后有个问题是：为什么frame_timer的更新是加上delay，而不是直接等于当前时间？

如果直接等于当前时间，那么其实frame_timer是变大了一点，那么在计算下一帧时间，也就是frame_timer+delay的时候，也就会大一点。而且每一帧都会是这个情况，最后每一帧都会大那么一点，整体而言可能会有有比较大的差别。测试视频25帧，1分多钟会有8s的差距，很大了。

```
if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX){
    is->frame_timer = time;
}
```

在frame_timer比较落后的时候，直接提到当前time上，就可以直接把状态修正，之后的播放又会走上正轨。

同步钟的概念： 音频或者视频，如果把内容正确的完整的播放，某个内容和一个时间是一一对应的，当前的音频或者视频播放到哪个位置，它就有一个时间来表示，这个时间就是同步钟的时间。所以音频钟的时间表示音频播放到哪个位置，视频钟表示播放到哪个位置。

因为音频和视频是分开表现的，就可能会出现音频和视频的进度不一致，在同步钟上就表现为两个同步钟的值不同，如果让两者统一，就是音视频同步的问题。

因为有了同步钟的概念，音视频内容上的同步就可以简化为更准确的：音频钟和视频钟时间相同。

这时会有一个同步钟作为主钟，也就是其他的同步钟根据这个主钟来调整自己的时间。满了就调快、快了就调慢。

`compute_target_delay`里的逻辑就是这样，`diff = get_clock(&is->vidclk) - get_master_clock(is);`这个是视频钟和主钟的差距：

```
//视频落后超过临界值，缩短下一帧时间
if (diff <= -sync_threshold)
    delay = FFMAX(0, delay + diff);
//视频超前，且超过临界值，延长下一帧时间
else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
    delay = delay + diff;
else if (diff >= sync_threshold)
    delay = 2 * delay;
```

至于为什么不都是`delay + diff`,即为什么还有第3种1情况，我的猜测是：

延时直接加上diff,那么下一帧就直接修正了视频种和主钟的差异，但有可能这个差异已经比较大了，直接一步到位的修正导致的效果就是：画面有明显的停顿，然后声音继续播，等到同步了视频再恢复正常。

而如果采用`2*delay`的方式，是每一次修正`delay`,多次逐步修正差异，可能变化上会更平滑一些。效果上就是画面和声音都是正常的，然后声音逐渐的追上声音，最后同步。

至于为什么第2种情况选择一步到位的修正，第3种情况选择逐步修正，这个很难说。因为AV_SYNC_FRAMEDUP_THRESHOLD值为0.15，对应的帧率是7左右，到这个程度，视频基本都是幻灯片了，我猜想这时逐步修正也没意义了。