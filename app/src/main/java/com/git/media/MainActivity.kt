package com.git.media

import android.content.Context
import android.graphics.SurfaceTexture
import android.os.Bundle
import android.util.Log
import android.view.Surface
import android.view.TextureView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {
    //var nativeMedia: NativeMedia = NativeMedia()
    companion object {
        val TAG = "MainActivity"
        val path ="/storage/emulated/0/Android/data/com.git.media/files/1.ts"// "/storage/emulated/0/Android/data/com.git.media/files/creat.m3u8"
        var goodPath: String? = null;
        val json =
            "[{\"time\":10.0,\"url\":\"video_0.ts\"},{\"time\":2.08,\"url\":\"video_1.ts\"}]"
    }

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
        var tv = findViewById<TextureView>(R.id.tv)
         tv.surfaceTextureListener = object : TextureView.SurfaceTextureListener {
             override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
                 NativeMedia.creatSurface(path, Surface(surface), width, height)

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
        if (result == 0) {
            NativeMedia.openStream(path)
        }
         Log.e(TAG, "result =$result")
        findViewById<TextView>(R.id.t).setOnClickListener { NativeMedia.closeStream(path) }
    }

    override fun onDestroy() {
        super.onDestroy()

    }
}