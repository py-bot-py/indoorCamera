import socket
import struct
import subprocess
import sys
import threading

# --- Configuration ---
TCP_IP = '0.0.0.0'       # Listen on all interfaces
ports = [1239, 1240, 1241, 1242, 1243, 1244, 1245]         # Port matching your ESP32
# Replace with your MediaMTX server IP/Port if not running locally
mediaMTXHost = 'rtsp://192.168.100.150:8554/'

streams = ['IceCream1', 'IceCream2', 'IceCream3', 'Cafe1', 'Cafe2', 'Cafe3', 'Stairway']

urls = [f'{mediaMTXHost}{stream}' for stream in streams]
# ---------------------

def start_ffmpeg(MEDIAMTX_RTSP_URL):
    """Starts FFmpeg to ingest MJPEG from stdin and output H264 RTSP to MediaMTX."""
    command = [
        'ffmpeg',
        '-y',
        '-f', 'image2pipe',       # Read raw images
        '-vcodec', 'mjpeg',       # Tell FFmpeg the input is JPEG
        '-i', '-',                # Read from standard input
        '-c:v', 'libx264',        # Encode to H.264 (best for WebRTC/MediaMTX compatibility)
        '-preset', 'ultrafast',   # Favor speed over compression for live streaming
        '-tune', 'zerolatency',   # Optimize for low latency
        '-pix_fmt', 'yuv420p',    # Standard pixel format for H264
        '-f', 'rtsp',             # Output as RTSP
        MEDIAMTX_RTSP_URL
    ]
    return subprocess.Popen(command, stdin=subprocess.PIPE)

def main(TCP_PORT, MEDIAMTX_RTSP_URL):
    ffmpeg_proc = None

    while True:
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((TCP_IP, TCP_PORT))
        server_socket.listen(1)

        print(f"Listening on port {TCP_PORT}...")

        try:
            conn, addr = server_socket.accept()
            print(f"Accepted connection from {addr}")

            ffmpeg_proc = start_ffmpeg(MEDIAMTX_RTSP_URL)

            while True:
                # --- read header ---
                length_bytes = conn.recv(4)
                if not length_bytes or len(length_bytes) < 4:
                    print("Client disconnected (header).")
                    break

                frame_length = struct.unpack('<I', length_bytes)[0]

                # --- read frame ---
                frame_data = bytearray()
                while len(frame_data) < frame_length:
                    packet = conn.recv(frame_length - len(frame_data))
                    if not packet:
                        print("Client disconnected (frame).")
                        break
                    frame_data.extend(packet)

                if len(frame_data) != frame_length:
                    print("Incomplete frame → dropping connection.")
                    break

                # --- ffmpeg write ---
                try:
                    ffmpeg_proc.stdin.write(frame_data)
                    ffmpeg_proc.stdin.flush()
                except Exception:
                    print("FFmpeg died → restarting connection.")
                    break

        except Exception as e:
            print(f"Server error: {e}")

        finally:
            try:
                conn.close()
            except:
                pass

            try:
                server_socket.close()
            except:
                pass

            if ffmpeg_proc and ffmpeg_proc.poll() is None:
                ffmpeg_proc.stdin.close()
                ffmpeg_proc.terminate()

            print("Resetting and waiting for new connection...")

if __name__ == "__main__":

    threads = []

    for port, url in zip(ports, urls):

        t = threading.Thread(
            target=main,
            args=(port, url),
            daemon=True
        )

        t.start()

        threads.append(t)

    for t in threads:
        t.join()