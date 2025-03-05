package com.git.media

import android.content.Context
import android.graphics.SurfaceTexture
import android.os.Bundle
import android.util.Log
import android.view.Surface
import android.view.TextureView
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {
    //var nativeMedia: NativeMedia = NativeMedia()
    companion object {
        val TAG = "MainActivity"
        val path = "rtmp://ns8.indexforce.com/home/mystream"//"http://kbs-dokdo.gscdn.com/dokdo_300/_definst_/dokdo_300.stream/playlist.m3u8"
    }

    fun dip2px(context: Context, dpValue: Float): Int {
        val scale = context.resources.displayMetrics.density
        return (dpValue * scale + 0.5f).toInt()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        val result = NativeMedia.init()
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
    }

}