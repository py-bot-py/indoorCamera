import socket
import struct
import subprocess
import threading
import time

# --- Configuration ---
TCP_IP = '0.0.0.0'
ports = [1239, 1240, 1241, 1242, 1243, 1244, 1245]

mediaMTXHost = 'rtsp://192.168.100.150:8554/'
streams = ['IceCream1', 'IceCream2', 'IceCream3', 'Cafe1', 'Cafe2', 'Cafe3', 'Stairway']

urls = [f'{mediaMTXHost}{stream}' for stream in streams]
# ---------------------


def start_ffmpeg(rtsp_url):
    command = [
        'ffmpeg',
        '-y',
        '-f', 'image2pipe',
        '-vcodec', 'mjpeg',
        '-i', '-',
        '-c:v', 'libx264',
        '-preset', 'ultrafast',
        '-tune', 'zerolatency',
        '-pix_fmt', 'yuv420p',
        '-f', 'rtsp',
        rtsp_url
    ]
    return subprocess.Popen(command, stdin=subprocess.PIPE)


def main(TCP_PORT, RTSP_URL):

    ffmpeg_proc = None

    while True:
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((TCP_IP, TCP_PORT))
        server_socket.listen(1)

        print(f"[{TCP_PORT}] Listening...")

        conn = None

        try:
            conn, addr = server_socket.accept()
            print(f"[{TCP_PORT}] Connected: {addr}")

            # IMPORTANT: enables watchdog via timeout exceptions
            conn.settimeout(1.0)

            ffmpeg_proc = start_ffmpeg(RTSP_URL)
            last_frame_time = time.time()

            while True:

                # --- 5 second watchdog ---
                if time.time() - last_frame_time > 5:
                    print(f"[{TCP_PORT}] No frames for 5s → restarting FFmpeg")
                    break

                try:
                    # --- header ---
                    length_bytes = conn.recv(4)
                    if not length_bytes or len(length_bytes) < 4:
                        print(f"[{TCP_PORT}] Client disconnected (header)")
                        break

                except socket.timeout:
                    continue

                frame_length = struct.unpack('<I', length_bytes)[0]

                # --- frame ---
                frame_data = bytearray()

                try:
                    while len(frame_data) < frame_length:
                        packet = conn.recv(frame_length - len(frame_data))

                        if not packet:
                            print(f"[{TCP_PORT}] Client disconnected (frame)")
                            break

                        frame_data.extend(packet)

                except socket.timeout:
                    continue

                if len(frame_data) != frame_length:
                    print(f"[{TCP_PORT}] Incomplete frame → restart")
                    break

                last_frame_time = time.time()

                # --- FFmpeg write ---
                try:
                    ffmpeg_proc.stdin.write(frame_data)
                    ffmpeg_proc.stdin.flush()
                except Exception:
                    print(f"[{TCP_PORT}] FFmpeg crashed → restart")
                    break

        except Exception as e:
            print(f"[{TCP_PORT}] Error: {e}")

        finally:
            try:
                if conn:
                    conn.close()
            except:
                pass

            try:
                server_socket.close()
            except:
                pass

            try:
                if ffmpeg_proc:
                    ffmpeg_proc.kill()
            except:
                pass

            print(f"[{TCP_PORT}] Restarting stream...\n")


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