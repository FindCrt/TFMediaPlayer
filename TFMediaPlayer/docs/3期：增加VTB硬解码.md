###流程


###值得注意的bug

1. 细小精度偏差带来的大问题

 `(5/4)*ysize` 改为 `ysize/4*5`就显示正常了，猜测是对的。5/4会引入小数，导致结果偏移，ysize是10万界别的大数，一点点的精度偏移最后可能会导致1个数的偏差，从而导致v分量的取值位置偏移，所有v分量的取值全部出错。
 
2. 硬解码的pixelBuffer的释放
 * pixelBuffer的内存用`CVPixelBufferRetain`和`CVPixelBufferRelease`来控制，逻辑应该还是引用只是那一套。
 * nv12不能直接播放，转到yuv420会复制内存，这部分内存需要释放
 