import socket
import struct
import cv2
import numpy as np
import subprocess
import sys

UDP_IP = "0.0.0.0"
UDP_PORT = 1239

# --- FFmpeg / MediaMTX Configuration ---
# Change this to your MediaMTX endpoint when ready
# Example: rtsp://localhost:8554/mystream
MEDIAMTX_URL = "rtsp://192.168.100.150:8554/esp32"
FPS = 15  # Adjust based on your ESP32's expected framerate

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.settimeout(0.2)

print(f"Listening on {UDP_PORT}...")

buffer = {}
ffmpeg_process = None
last_valid_frame = None

def start_ffmpeg(width, height):
    """Spawns the FFmpeg process optimized for zero-latency live streaming."""
    command = [
        'ffmpeg',
        '-y',                  # Overwrite outp
        '-f', 'rawvideo',      # Input format is raw video
        '-vcodec', 'rawvideo',
        '-pix_fmt', 'bgr24',    # OpenCV uses BGR by default
        '-s', f"{width}x{height}",
        '-r', str(FPS),        # TELL FFMPEG THE EXPECTED INPUT FRAMERATE (Crucial!)
        '-i', '-',             # Read from stdin

        # Output encoding (Optimized for ultra-low latency)
        '-c:v', 'libx264',
        '-preset', 'ultrafast', 
        '-tune', 'zerolatency', 
        '-g', str(FPS * 2),    # Force a keyframe every 2 seconds so Frigate recovers quickly
        '-pix_fmt', 'yuv420p', 
        '-f', 'rtsp',          # Output format
        '-rtsp_transport', 'tcp', # Force TCP for RTSP delivery to avoid losing data twice
        MEDIAMTX_URL
    ]
    return subprocess.Popen(command, stdin=subprocess.PIPE)

def send_to_ffmpeg(frame):
    """Safely writes a frame to the FFmpeg pipe."""
    global ffmpeg_process
    if ffmpeg_process is not None:
        try:
            ffmpeg_process.stdin.write(frame.tobytes())
        except Exception as e:
            print(f"FFmpeg pipe error (is MediaMTX running?): {e}")


def decode_and_pipe(frame_buffer):
    """Stitches chunks, decodes via OpenCV (repairing errors), and pipes to FFmpeg."""
    global ffmpeg_process, last_valid_frame

    if not frame_buffer:
        return

    # If chunk 0 is missing, the JPEG header is gone.
    if 0 not in frame_buffer:
        print("Dropped frame: Missing initial chunk (JPEG header)")
        # Trick for fragile streams: Repeat the last valid frame to maintain FPS
        if last_valid_frame is not None:
            send_to_ffmpeg(last_valid_frame)
        return

    # Sort chunks by index and join them
    sorted_indices = sorted(frame_buffer.keys())
    data = b"".join(frame_buffer[i] for i in sorted_indices)

    # OpenCV elegantly handles truncated JPEGs by filling the missing bottom with gray/green
    img = cv2.imdecode(np.frombuffer(data, dtype=np.uint8), cv2.IMREAD_COLOR)

    if img is not None:
        height, width, _ = img.shape

        # Start FFmpeg dynamically once we know the stream resolution
        if ffmpeg_process is None:
            print(f"Starting FFmpeg... Detected Resolution: {width}x{height}")
            ffmpeg_process = start_ffmpeg(width, height)

        send_to_ffmpeg(img)
        last_valid_frame = img  # Save frame in case the next one drops

    else:
        print("Failed to decode frame: Data too corrupted")
        # Keep stream alive by repeating the last known good frame
        if last_valid_frame is not None:
            send_to_ffmpeg(last_valid_frame)


while True:
    try:
        packet, addr = sock.recvfrom(65536)

        # -------------------------
        # HEADER PACKET (Signals a new frame is starting)
        # -------------------------
        if packet.startswith(b"=="):
            # 1. Decode, repair, and pipe the PREVIOUS frame
            decode_and_pipe(buffer)

            # 2. Flush buffer for the incoming frame
            buffer = {}

            # 3. Send ACK back to ESP32
            sock.sendto(b"ok", addr)
            continue

        # -------------------------
        # FRAME DATA PACKET
        # -------------------------
        if len(packet) < 4:
            continue

        # Extract 4-byte chunk index and raw image payload
        idx = struct.unpack("I", packet[:4])[0]
        data = packet[4:]

        buffer[idx] = data

    except socket.timeout:
        # If the ESP32 stops sending or a frame gets cut off, push what we have
        if buffer:
            decode_and_pipe(buffer)
            buffer = {}
    except KeyboardInterrupt:
        print("\nShutting down...")
        if ffmpeg_process:
            ffmpeg_process.stdin.close()
            ffmpeg_process.wait()
        break
    except Exception as e:
        print(f"Error: {e}")
