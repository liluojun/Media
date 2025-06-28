package com.git.media

import android.content.Context
import android.graphics.SurfaceTexture
import android.media.MediaCodec
import android.media.MediaFormat
import android.os.Bundle
import android.util.Log
import android.view.Surface
import android.view.TextureView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder


class MainActivity : AppCompatActivity() {
    //var nativeMedia: NativeMedia = NativeMedia()
    companion object {
        val TAG = "MainActivity"
        val path =
            "/storage/emulated/0/Android/data/com.git.media/files/4.ts"// "/storage/emulated/0/Android/data/com.git.media/files/creat.m3u8"
        var goodPath: String? = null;
        val json =
            "[{\"time\":10.0,\"url\":\"video_0.ts\"},{\"time\":2.08,\"url\":\"video_1.ts\"}]"
        var picPath = "/storage/emulated/0/Android/data/com.git.media/files/${System.currentTimeMillis()}.png"
    }

    var width: Int = 0
    var height: Int = 0
    lateinit var tv: TextureView
    fun dip2px(context: Context, dpValue: Float): Int {
        val scale = context.resources.displayMetrics.density
        return (dpValue * scale + 0.5f).toInt()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        val result = NativeMedia.init()
        goodPath = this.getExternalFilesDir(null)?.getAbsolutePath()
        /* var m3u8 = NativeMedia.creatM3u8File(goodPath as String, json)
         if (!m3u8.isEmpty()) {
             NativeMedia.m3u8ToMp4(/*m3u8*/"/storage/emulated/0/Android/data/com.git.media/files/creat.m3u8",
                 "${(goodPath as String)}/test.mp4")
         }*/
        tv = findViewById<TextureView>(R.id.tv)
        tv.surfaceTextureListener = object : TextureView.SurfaceTextureListener {
            override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
                this@MainActivity.width = width
                this@MainActivity.height = height
            }

            override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {
                NativeMedia.changeSurfaceSize(path, width, height)
            }

            override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
                NativeMedia.destorySurface(path)
                return true
            }

            override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {

            }
        }

        Log.e(TAG, "result =$result")
        findViewById<TextView>(R.id.t).setOnClickListener {
            if (result == 0) {
                NativeMedia.openStream(path)
                NativeMedia.creatSurface(path, Surface(tv.surfaceTexture), width, height)

            }
        }
        findViewById<TextView>(R.id.t1).setOnClickListener { test()/* NativeMedia.closeStream(path) */ }
        findViewById<TextView>(R.id.t2).setOnClickListener { NativeMedia.playbackSpeed(path, 2.0) }
        findViewById<TextView>(R.id.t3).setOnClickListener {
            Toast.makeText(this, "保存成功", Toast.LENGTH_SHORT).show()
            picPath = "/storage/emulated/0/Android/data/com.git.media/files/${System.currentTimeMillis()}.png"
            NativeMedia.screenshot(path, picPath)
        }

    }

    val vps = byteArrayOf(0x00.toByte(), 0x00.toByte(), 0x01.toByte(), 0x40.toByte(),
        0x01.toByte(), 0x0C.toByte(), 0x01.toByte(), 0xFF.toByte(), 0xFF.toByte(),
        0x01.toByte(), 0x60.toByte(), 0x00.toByte(), 0x00.toByte(), 0x03.toByte(),
        0x00.toByte(), 0x90.toByte(), 0x00.toByte(), 0x00.toByte(), 0x03.toByte(),
        0x00.toByte(), 0x00.toByte(), 0x03.toByte(), 0x00.toByte(), 0x96.toByte(),
        0xBA.toByte(), 0x02.toByte(), 0x40.toByte())
    val pps = byteArrayOf(0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x01.toByte(), 0x42.toByte(),
        0x01.toByte(), 0x01.toByte(), 0x01.toByte(), 0x60.toByte(), 0x00.toByte(), 0x00.toByte(),
        0x03.toByte(), 0x00.toByte(), 0x90.toByte(), 0x00.toByte(), 0x00.toByte(), 0x03.toByte(),
        0x00.toByte(), 0x00.toByte(), 0x03.toByte(), 0x00.toByte(), 0x96.toByte(), 0xA0.toByte(),
        0x01.toByte(), 0x68.toByte(), 0x20.toByte(), 0x06.toByte(), 0x59.toByte(), 0xF7.toByte(),
        0x96.toByte(), 0xEA.toByte(), 0x49.toByte(), 0x30.toByte(), 0x88.toByte(), 0x04.toByte(),
        0x00.toByte(), 0x00.toByte(), 0x03.toByte(), 0x00.toByte(), 0x04.toByte(), 0x00.toByte(),
        0x00.toByte(), 0x03.toByte(), 0x00.toByte(), 0x64.toByte(), 0x20.toByte())
    val sps = byteArrayOf(0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x01.toByte(), 0x44.toByte(),
        0x01.toByte(), 0xC0.toByte(), 0x72.toByte(), 0xF0.toByte(), 0x22.toByte(), 0x40)
    var frameIndex = 0
    lateinit var codec: MediaCodec
    fun test() {
        try {
            codec = MediaCodec.createDecoderByType("video/hevc")
            val format = MediaFormat.createVideoFormat("video/hevc", 2880, 1620)
            format.setByteBuffer("csd-0", ByteBuffer.wrap(vps)) // 通常是 VPS
            format.setByteBuffer("csd-1", ByteBuffer.wrap(pps))
            format.setByteBuffer("csd-2", ByteBuffer.wrap(sps)) // H.265 可以有多个 csd
            format.setInteger(MediaFormat.KEY_FRAME_RATE, 25);
            codec.configure(format, Surface(tv.surfaceTexture), null, 0);
            codec.start();
            processFile(path)

        } catch (e: Exception) {
            Log.e("test", e.message.toString())
        }
    }

    fun t(result: ByteArray) {
        val bufferIndex = codec.dequeueInputBuffer(10000)
        if (bufferIndex >= 0) {
            val inputBuffer = codec.getInputBuffer(bufferIndex)
            inputBuffer?.put(result)
            val ptsUs = frameIndex * 1_000_000L / 30
            codec.queueInputBuffer(bufferIndex, 0, result.size, ptsUs, 0)
        }

        val info = MediaCodec.BufferInfo()
        val outIndex = codec.dequeueOutputBuffer(info, 10000)
        if (outIndex >= 0) {
            codec.releaseOutputBuffer(outIndex, true) // render to Surface
        }
    }

    fun processFile(path: String): Int {
        var totalSent = 0L
        var flag = true

        val file = File(path)
        if (!file.exists()) {
            Log.e("FileProcess", "file error: $path")
            return -1
        }

        file.inputStream().use { input ->
            val header = ByteArray(36)
            while (true) {

                // 尝试读取 36 字节头部
                val headerRead = input.readNBytes(header, 36)
                if (headerRead.size < 36) {
                    Log.e("FileProcess", "! 文件头不完整: $path 读取到 $headerRead 字节")
                    break
                }

                // 提取 chunk_size 和 time（对应 header[20..23] 和 header[12..15]）
                val chunkSize = ByteBuffer.wrap(header, 20, 4).order(ByteOrder.LITTLE_ENDIAN).int
                val time = ByteBuffer.wrap(header, 12, 4).order(ByteOrder.LITTLE_ENDIAN).int
                if (flag) {
                    flag = false
                    Log.e("FileProcess", "time=$time")
                }

                // 读取 chunkSize 大小的数据帧体
                val body = ByteArray(chunkSize)
                val bodyRead = input.readNBytes(body, chunkSize)
                val actualRead = bodyRead.size

                // 拼接完整数据
                val fullPacket = ByteArray(36 + actualRead)
                System.arraycopy(header, 0, fullPacket, 0, 36)
                System.arraycopy(bodyRead, 0, fullPacket, 36, actualRead)
                frameIndex++
                t(fullPacket)
                totalSent += fullPacket.size
            }
        }

        Log.e("FileProcess", "√ 文件处理完成: $path    总发送: $totalSent bytes")
        return 0
    }

    fun InputStream.readNBytes(buffer: ByteArray, len: Int): ByteArray {
        var offset = 0
        while (offset < len) {
            val read = this.read(buffer, offset, len - offset)
            if (read == -1) break
            offset += read
        }
        return buffer.copyOf(offset)
    }

    override fun onDestroy() {
        super.onDestroy()

    }
}