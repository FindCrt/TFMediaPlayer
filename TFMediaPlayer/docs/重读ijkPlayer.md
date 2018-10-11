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