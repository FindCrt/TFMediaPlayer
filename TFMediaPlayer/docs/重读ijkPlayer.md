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


nv12可以直接显示，使用的是双元素的纹理，纹理构建时使用