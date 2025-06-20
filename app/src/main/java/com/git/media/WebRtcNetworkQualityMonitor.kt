package com.git.media

import android.content.Context
import android.util.Log
import kotlinx.coroutines.*
//import okhttp3.*
//import org.webrtc.*
import java.util.concurrent.TimeUnit

class WebRtcNetworkQualityMonitor(
    private val context: Context,
    private val signalingUrl: String // WebSocket 信令服务器地址
) {
  /*  private val TAG = "WebRtcNetQuality"
    private val scope = CoroutineScope(Dispatchers.Main)

    private var peerConnectionFactory: PeerConnectionFactory? = null
    private var peerConnection: PeerConnection? = null
    private var webSocket: WebSocket? = null
    private var statsJob: Job? = null

    private val iceServers = listOf(
        //stun:stun.l.google.com:19302
        //stun:stun.anyrtc.io:347
        PeerConnection.IceServer.builder("stun:your.stun.server:3478").createIceServer(),
        // 如果有 TURN，放这里
    )

    private val client = OkHttpClient.Builder()
        .readTimeout(0, TimeUnit.MILLISECONDS)
        .build()

    fun start() {
        initPeerConnectionFactory()
        connectSignaling()
    }

    fun stop() {
        statsJob?.cancel()
        webSocket?.close(1000, null)
        peerConnection?.close()
        peerConnectionFactory?.dispose()
        peerConnection = null
        peerConnectionFactory = null
    }

    private fun initPeerConnectionFactory() {
        val options = PeerConnectionFactory.InitializationOptions.builder(context)
            .createInitializationOptions()
        PeerConnectionFactory.initialize(options)
        peerConnectionFactory = PeerConnectionFactory.builder().createPeerConnectionFactory()
    }

    private fun connectSignaling() {
        val request = Request.Builder().url(signalingUrl).build()
        webSocket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                Log.d(TAG, "Signaling connected")
                createPeerConnection()
                createAndSendOffer()
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                handleSignalingMessage(text)
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                Log.e(TAG, "Signaling failure: ${t.message}")
            }
        })
    }

    private fun createPeerConnection() {
        val rtcConfig = PeerConnection.RTCConfiguration(iceServers)
        rtcConfig.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN
        peerConnection = peerConnectionFactory?.createPeerConnection(rtcConfig, object : PeerConnection.Observer {
            override fun onIceCandidate(candidate: IceCandidate?) {
                candidate?.let {
                    val json = """{"type":"candidate","candidate":${toJson(it)}}"""
                    webSocket?.send(json)
                }
            }

            override fun onIceConnectionChange(newState: PeerConnection.IceConnectionState?) {
                Log.d(TAG, "ICE connection state: $newState")
                if (newState == PeerConnection.IceConnectionState.CONNECTED) {
                    startStatsCollection()
                }
            }

            override fun onSignalingChange(newState: PeerConnection.SignalingState?) {}
            override fun onIceGatheringChange(newState: PeerConnection.IceGatheringState?) {}
            override fun onAddStream(stream: MediaStream?) {}
            override fun onRemoveStream(stream: MediaStream?) {}
            override fun onDataChannel(dc: DataChannel?) {}
            override fun onRenegotiationNeeded() {}
            override fun onAddTrack(receiver: RtpReceiver?, streams: Array<out MediaStream>?) {}
        })
    }

    private fun createAndSendOffer() {
        peerConnection?.createOffer(object : SdpObserver {
            override fun onCreateSuccess(sdp: SessionDescription) {
                peerConnection?.setLocalDescription(object : SdpObserver {
                    override fun onSetSuccess() {
                        val json = """{"type":"offer","sdp":"${sdp.description}"}"""
                        webSocket?.send(json)
                    }
                    override fun onSetFailure(p0: String?) {}
                    override fun onCreateSuccess(p0: SessionDescription?) {}
                    override fun onCreateFailure(p0: String?) {}
                }, sdp)
            }

            override fun onCreateFailure(error: String?) {
                Log.e(TAG, "Create offer failed: $error")
            }

            override fun onSetSuccess() {}
            override fun onSetFailure(error: String?) {}
        }, MediaConstraints())
    }

    private fun handleSignalingMessage(message: String) {
        // 简单示例，建议用 JSON 解析库
        when {
            message.contains("\"type\":\"answer\"") -> {
                val sdp = extractSdp(message)
                peerConnection?.setRemoteDescription(object : SdpObserver {
                    override fun onSetSuccess() {}
                    override fun onSetFailure(p0: String?) {}
                    override fun onCreateSuccess(p0: SessionDescription?) {}
                    override fun onCreateFailure(p0: String?) {}
                }, SessionDescription(SessionDescription.Type.ANSWER, sdp))
            }
            message.contains("\"type\":\"candidate\"") -> {
                val candidate = extractCandidate(message)
                peerConnection?.addIceCandidate(candidate)
            }
        }
    }

    private fun startStatsCollection() {
        statsJob = scope.launch {
            while (isActive) {
                peerConnection?.getStats { report ->
                    report.statsMap.values.forEach { stat ->
                        when (stat.type) {
                            "candidate-pair" -> {
                                val rtt = stat.members["currentRoundTripTime"]?.toDoubleOrNull() ?: 0.0
                                Log.d(TAG, "RTT: ${(rtt * 1000).toInt()} ms")
                            }
                            "inbound-rtp" -> {
                                val loss = stat.members["packetsLost"] ?: "0"
                                val jitter = stat.members["jitter"] ?: "0"
                                Log.d(TAG, "Packets lost: $loss, Jitter: $jitter")
                            }
                        }
                    }
                }
                delay(3000)
            }
        }
    }

    private fun toJson(candidate: IceCandidate): String {
        return """{
            "candidate": "${candidate.sdp}",
            "sdpMid": "${candidate.sdpMid}",
            "sdpMLineIndex": ${candidate.sdpMLineIndex}
        }"""
    }

    private fun extractSdp(json: String): String {
        // 简单示例，不完整，请用 JSON 解析
        val regex = """"sdp":"(.*?)"""".toRegex()
        return regex.find(json)?.groups?.get(1)?.value ?: ""
    }

    private fun extractCandidate(json: String): IceCandidate {
        // 简单示例，务必用 JSON 库解析
        val candidateRegex = """"candidate":"(.*?)"""".toRegex()
        val sdpMidRegex = """"sdpMid":"(.*?)"""".toRegex()
        val sdpMLineIndexRegex = """"sdpMLineIndex":(\d+)""".toRegex()

        val candidate = candidateRegex.find(json)?.groups?.get(1)?.value ?: ""
        val sdpMid = sdpMidRegex.find(json)?.groups?.get(1)?.value ?: ""
        val sdpMLineIndex = sdpMLineIndexRegex.find(json)?.groups?.get(1)?.value?.toInt() ?: 0

        return IceCandidate(sdpMid, sdpMLineIndex, candidate)
    }*/
}