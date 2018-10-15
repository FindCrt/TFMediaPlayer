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


**1. 视频显示时的时间控制**

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

一开始当前帧播放时间没到，goto display等待下次循环，循环多次，时间不段后移，终于播放时间到了,播放当前帧，frame_timer更新为当前帧的时间。然后又重复上面的过程，去播放下一帧。

然后有个问题是：为什么frame_timer的更新是加上delay，而不是直接等于当前时间？

如果直接等于当前时间，因为`time>= frame_timer+delay`,那么frame_timer是相对更大了一些，那么在计算下一帧时间，也就是frame_timer+delay的时候，也就会大一点。而且每一帧都会是这个情况，最后每一帧都会大那么一点，整体而言可能会有有比较大的差别。

```
if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX){
    is->frame_timer = time;
}
```

在frame_timer比较落后的时候，直接提到当前time上，就可以直接把状态修正，之后的播放又会走上正轨。

**2. 同步钟以及钟时间的修正**

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

**3. 同步钟时间获取的实现**

再看同步钟时间的实现：`get_clock`获取时间， `set_clock_at`更新时间。

解析一下：`return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);`，为啥这么写?

上一次显示的时候，更新了同步钟，调用`set_clock_at `,上次的时间为c->last_updated,则：


`c->pts_drift + time = (c->pts - c->last_updated)+time;`

假设距离上次的时间差`time_diff = time - c->last_updated`，则表达式整体可以变为：

`c->pts+time_diff+(c->speed - 1)*time_diff`,合并后两项变为:
`c->pts+c->speed*time_diff`.

我们要求得就是当前时间时的媒体内容位置，上次的位置是`c->pts`,而中间过去了`time_diff`这么多时间，媒体内容过去的时间就是：播放速度x现实时间，也就是c->speed*time_diff。举例：现实里过去10s,如果你2倍速的播放，那视频就过去了20s。所以这个表达式就很清晰了。

在`set_clock_speed`里同时调用了`set_clock`,这是为了保证从上次更新时间以来，速度是没变的，否则计算就没有意义了。

到这差不多了，还有一点是在seek时候同步钟的处理，到seek问题的时候再看。

#####seek的处理

seek就是调整进度条到新的地方开始播，这个操作会打乱原本的数据流，一些播放秩序要重新建立。需要处理的问题包括：

* 缓冲区数据的释放，而且要重头到位全部释放干净
* 播放时间显示
* “加载中”的状态的维护,这个影响着用户界面的显示问题
* 剔除错误帧的问题


**流程**

1. 外界seek调用到`ijkmp_seek_to_l`，然后发送消息`ffp_notify_msg2(mp->ffplayer, FFP_REQ_SEEK, (int)msec);`,消息捕获到后调用到`stream_seek`,然后设置`seek_req`为1，记录seek目标到`seek_pos`。
2. 在读取函数`read_thread`里，在`is->seek_req`为true时，进入seek处理,几个核心处理:
  * `ffp_toggle_buffering`关闭解码，packet缓冲区静止
  * 调用`avformat_seek_file`进行seek
  * 成功之后用`packet_queue_flush`清空缓冲区，并且把`flush_pkt`插入进去，这时一个标记数据
  * 把当前的serial记录下来

到这里值得学习的点是：

* 我在处理seek的时候，是另开一个线程调用了ffmpeg的seek方法，而这里是直接在读取线程里，这样就不用等待读取流程的结束了
* seek成功之后再flush缓冲区

因为

```
if (pkt == &flush_pkt)
        q->serial++;
```
所以serial的意义就体现出来了，每次seek,serial+1,也就是serial作为一个标记，相同代表是同一次seek里的。

3. 到`decoder_decode_frame`里：
 * 因为seek的修改是在读取线程里，和这里的解码线程不是一个，所以seek的修改可以在这里代码的任何位置出现。
 * `if (d->queue->serial == d->pkt_serial)`这个判断里面为代码块1，`while (d->queue->serial != d->pkt_serial)`这个循环为代码块2，`if (pkt.data == flush_pkt.data)`这个判断为true为代码块3，false为代码块4.
 * 如果seek修改出现在代码块2之前，那么就一定会进代码块2，因为`packet_queue_get_or_buffering`会一直读取到flush_pkt，所以也就会一定进代码块3，会执行`avcodec_flush_buffers`清空解码器的缓存。
 * 如果seek在代码块2之后，那么就只会进代码块4，但是再循环回去时，会进代码块2、代码块3，然后`avcodec_flush_buffers`把这个就得packet清掉了。
 * 综合上面两种情况，只有seek之后的packet才会得到解码，牛逼！

这一段厉害在：

 * seek的修改在任何时候，它都不会出错
 * seek的处理是在解码线程里做的，省去了条件锁等线程间通信的处理，更简单稳定。如果整个数据流是一条河流，那flush_pkt就像一个这个河流的一个浮标，遇到这个浮标，后面水流的颜色都变了。有一种自己升级自己的这种意思，而不是由一个第三方来做辅助的升级。对于流水线式的程序逻辑，这样做更好。

 
4. 播放处
 
 视频`video_refresh`里：
 
 ```
	if (vp->serial != is->videoq.serial) {
	    frame_queue_next(&is->pictq);
	    goto retry;
	}
 ```
 音频`audio_decode_frame`里：
 
 ```
	 do {
	    if (!(af = frame_queue_peek_readable(&is->sampq)))
	        return -1;
	    frame_queue_next(&is->sampq);
	 } while (af->serial != is->audioq.serial);
 ```
 都根据serial把旧数据略过了。
 
 所以整体看下来，seek体系里最厉害的东西的东西就是使用了serial来标记数据，从而可以很明确的知道哪些是就数据，哪些是新数据。然后处理都是在原线程里做的处理，而不是在另外的线程里来修改相关的数据，省去了线程控制、线程通讯的麻烦的操作，稳定性也提高了。
 
 
**播放时间获取**

看`ijkmp_get_current_position`,seek时，返回seek的时间，播放时看`ffp_get_current_position_l`,核心就是内容时间`get_master_clock`减去开始时间`is->ic->start_time`。

seek的时候，内容位置发生了一个巨大的跳跃，所以要怎么维持同步钟的正确？

* 音频和视频数据里的pts都是`frame->pts * av_q2d(tb)`,也就是内容时间，但是转成了现实时间单位。
* 然后`is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;`,所以`is->audio_clock`是最新的一帧音频的数据**播完**时内容时间
* 在音频的填充方法里，设置音频钟的代码是：

 ```
 set_clock_at(&is->audclk, 
 is->audio_clock - (double)(is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec - SDL_AoutGetLatencySeconds(ffp->aout), 
 is->audio_clock_serial, 
 ffp->audio_callback_time / 1000000.0);
 ```
 
 因为`is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;`,所以`audio_write_buf_size`就是当前帧还没读完剩余的大小，所以`(double)(is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec`就标识剩余的数据播放完的时间。
 
 `SDL_AoutGetLatencySeconds(ffp->aout)`是上层的缓冲区的数据的时间，对iOS的AudioQueue而言，有多个AudioBuffer等待播放，这个时间就是它们播放完要花的时间。
 
 时间轴上是这样的：
 
 [帧结束点][剩余buf时间][上层的buf时间][刚结束播放的点]
 
所以第二个参数的时间是：当前帧结束时的内容时间-剩余buf的时间-上层播放器buf的时间，也就是刚结束播放的内容时间。

`ffp->audio_callback_time`是填充方法调用时的时间，这里存在一个假设，就是上层播放器播完了一个buffer，立马调用了填充函数，所以`ffp->audio_callback_time`就是刚结束播放的现实时间。

这样第2个参数和第4个参数的意义就匹配上了。

回到seek,在seek完成后，会有第一个新的frame进入播放，它会把同步钟的pts，也就是媒体的内容时间调整到seek后的位置，那么还有一个问题：`mp->seek_req`这个标识重置回0的时间点必须比第一个新frame的`set_clock_at`要晚，否则同步钟的时间还没调到新的，seek的标识就结束了，然后根据同步钟去计算当前的播放时间，就出错了(界面上应该是进度条闪回seek之前)。

**而事实上并没有这样**，因为在同步钟的`get_clock`,还有一个

```
if (*c->queue_serial != c->serial)
        return NAN;
```
这个serial真是神操作，太好用了！

音频钟和视频钟的serial都是在播放时更新的，也就是第一帧新数据播放时更新到seek以后的serial,而`c->queue_serial`是一个指针：`init_clock(&is->vidclk, &is->videoq.serial);`,和packetQueue的serial共享内存的。

所以也就是到第一帧新数据播放后，`c->queue_serial != c->serial`这个才不成立。也就是即使`mp->seek_req`重置回0，取得值还是seek的目标值，还不是根据pts计算的，所以也不会闪回了。

关于seek的东西太复杂了。