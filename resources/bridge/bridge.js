import { WebSocketServer, WebSocket as NodeWebSocket } from 'ws';
import wrtc from '@koush/wrtc';
import { createDecartClient, models } from "@decartai/sdk";
import sharp from 'sharp';

const { RTCVideoSource, RTCVideoSink } = wrtc.nonstandard;

// ===== POLYFILLS =====
global.RTCPeerConnection = wrtc.RTCPeerConnection;
global.RTCSessionDescription = wrtc.RTCSessionDescription;
global.RTCIceCandidate = wrtc.RTCIceCandidate;
global.MediaStream = wrtc.MediaStream;
global.WebSocket = NodeWebSocket;

// ===== SERVER =====
const wss = new WebSocketServer({ port: 8080 });
console.log("🚀 Internal Bridge Engine Ready");

wss.on('connection', (ws) => {
    console.log("✅ Client connected");

    let videoSource = null;
    let videoTrack = null;
    let localStream = null;

    let decartSession = null;
    let videoSink = null;

    let isStreaming = false;
    let isStarting = false;
    let isShuttingDown = false;

    // ================= CLEANUP =================
    const cleanup = async () => {
        if (isShuttingDown) return;

        console.log("🛑 Cleaning session...");
        isStreaming = false;
        isStarting = false;

        try {
            if (videoSink) {
                try { videoSink.stop(); } catch { }
                videoSink = null;
            }

            if (decartSession) {
                try { await decartSession.disconnect(); } catch { }
                decartSession = null;
            }

            if (videoTrack) {
                try { videoTrack.stop(); } catch { }
                videoTrack = null;
            }

            videoSource = null;
            localStream = null;

            // allow WebRTC to release resources
            await new Promise(r => setTimeout(r, 200));

        } catch (e) {
            console.log("Cleanup error:", e.message);
        }

        console.log("✅ Cleanup complete");
    };

    // ================= START =================

    const startSession = async (msg) => {
        if (isStarting || isStreaming || isShuttingDown) {
            console.log("⚠️ Start ignored");
            return;
        }

        isStarting = true;
        await cleanup();

        console.log("🧠 Starting AI session...");

        if (!msg.image || !msg.user_id) {
            console.log("❌ Missing image or user_id");
            isStarting = false;
            return;
        }

        let base64 = msg.image.includes(",")
            ? msg.image.split(",")[1]
            : msg.image;

        try {
            // ===============================
            // GET SECURE TOKEN FROM VERCEL
            // ===============================
            const tokenRes = await fetch(
                "https://origin-api-gilt.vercel.app/api/token",
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify({
                        user_id: msg.user_id
                    })
                }
            );

            const tokenJson = await tokenRes.json();

            if (!tokenJson.success || !tokenJson.apiKey) {
                console.log("❌ Token denied");
                isStarting = false;
                return;
            }

            console.log("✅ Secure token granted");

            // ===============================
            // VIDEO PIPELINE
            // ===============================
            videoSource = new RTCVideoSource();
            videoTrack = videoSource.createTrack();
            localStream = new wrtc.MediaStream([videoTrack]);

            const client = createDecartClient({
                apiKey: tokenJson.apiKey
            });

            decartSession = await client.realtime.connect(localStream, {
                model: models.realtime("lucy-2"),

                initialState: {
                    prompt: {
                        text: "Strongly transform the person in the video to match the character in the reference image. Fully adopt identity, facial features, and style.",
                        enhance: true
                    },
                    image: base64,
                    run: true
                },

                onRemoteStream: (aiStream) => {
                    console.log("🔥 AI Ready");

                    ws.send(JSON.stringify({
                        type: "connected"
                    }));

                    videoSink = new RTCVideoSink(
                        aiStream.getVideoTracks()[0]
                    );

                    isStreaming = true;
                    isStarting = false;

                    videoSink.onframe = async ({ frame }) => {
                        if (!isStreaming || !frame || !frame.data) return;
                        if (ws.readyState !== 1) return;

                        try {
                            const { width, height, data } = frame;

                            const rgba = Buffer.allocUnsafe(width * height * 4);

                            const ySize = width * height;
                            const uvSize = (width / 2) * (height / 2);

                            const Y = data.subarray(0, ySize);
                            const U = data.subarray(ySize, ySize + uvSize);
                            const V = data.subarray(ySize + uvSize);

                            let idx = 0;

                            for (let j = 0; j < height; j++) {
                                for (let i = 0; i < width; i++) {
                                    const yIdx = j * width + i;
                                    const uvIdx =
                                        Math.floor(j / 2) * (width / 2) +
                                        Math.floor(i / 2);

                                    const y = Y[yIdx];
                                    const u = U[uvIdx] - 128;
                                    const v = V[uvIdx] - 128;

                                    rgba[idx++] = Math.max(0, Math.min(255, y + 1.402 * v));
                                    rgba[idx++] = Math.max(0, Math.min(255, y - 0.344 * u - 0.714 * v));
                                    rgba[idx++] = Math.max(0, Math.min(255, y + 1.772 * u));
                                    rgba[idx++] = 255;
                                }
                            }

                            const jpeg = await sharp(rgba, {
                                raw: { width, height, channels: 4 }
                            })
                                .jpeg({ quality: 90 })
                                .toBuffer();

                            ws.send(jpeg);

                        } catch { }
                    };
                }
            });

            console.log("✅ Session created");

        } catch (err) {
            console.log("❌ Start failed:", err.message);
            isStarting = false;
            await cleanup();
        }
    };

    // ================= MESSAGE =================
    ws.on('message', async (data) => {

        // ==== CONTROL ====
        try {
            const msg = JSON.parse(data.toString());

            if (msg.type === "start") {
                console.log("📩 START received");
                await startSession(msg);
                return;
            }

            if (msg.type === "stop") {
                console.log("📩 STOP received");

                isShuttingDown = true;

                await cleanup();

                console.log("💀 Shutting down bridge (one-shot)");

                process.exit(0); // EXACTLY what you want
            }

        } catch { }

        // ==== FRAME INPUT ====
        if (
            Buffer.isBuffer(data) &&
            isStreaming &&
            !isShuttingDown &&
            videoSource
        ) {
            try {
                const { data: rgba, info } = await sharp(data)
                    .ensureAlpha()
                    .raw()
                    .toBuffer({ resolveWithObject: true });

                const { width, height } = info;

                const Y = new Uint8Array(width * height);
                const U = new Uint8Array((width / 2) * (height / 2));
                const V = new Uint8Array((width / 2) * (height / 2));

                let yIdx = 0, uIdx = 0, vIdx = 0;

                for (let j = 0; j < height; j++) {
                    for (let i = 0; i < width; i++) {
                        const idx = (j * width + i) * 4;

                        const r = rgba[idx];
                        const g = rgba[idx + 1];
                        const b = rgba[idx + 2];

                        Y[yIdx++] = 0.257 * r + 0.504 * g + 0.098 * b + 16;

                        if (j % 2 === 0 && i % 2 === 0) {
                            U[uIdx++] = -0.148 * r - 0.291 * g + 0.439 * b + 128;
                            V[vIdx++] = 0.439 * r - 0.368 * g - 0.071 * b + 128;
                        }
                    }
                }

                const i420 = new Uint8Array(Y.length + U.length + V.length);
                i420.set(Y, 0);
                i420.set(U, Y.length);
                i420.set(V, Y.length + U.length);

                videoSource.onFrame({ width, height, data: i420 });

            } catch { }
        }
    });

    ws.on('close', async () => {
        console.log("❌ Client disconnected");
        isShuttingDown = true;
        await cleanup();
    });
});