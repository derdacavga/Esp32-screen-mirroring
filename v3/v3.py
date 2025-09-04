import asyncio
import websockets
import mss
from PIL import Image
import io
from pynput.mouse import Button, Controller

ESP32_IP = "192.168.1.112" ## Change your Esp32 IP Adress
ESP32_PORT = 81
MONITOR_NUMBER = 2

TARGET_FPS = 12
JPEG_QUALITY = 35
CHUNK_SIZE = 4096*3
RESIZE_METHOD = Image.Resampling.BILINEAR

mouse = Controller()

async def handle_touch(websocket):
    try:
        async for message in websocket:
            if message.startswith("TOUCH:"):
                try:
                    coords = message.strip().split(':')[1]
                    x_touch, y_touch = map(int, coords.split(','))
                    with mss.mss() as sct:
                        monitor = sct.monitors[MONITOR_NUMBER]
                        pc_screen_width = monitor["width"]
                        pc_screen_height = monitor["height"]
                    target_x = (x_touch / 320) * pc_screen_width
                    target_y = (y_touch / 240) * pc_screen_height
                    mouse.position = (int(target_x), int(target_y))
                    # mouse.click(Button.left, 1) # If you want touch touch = click enable this line
                except Exception:
                    pass
            elif message == "FRAME_DONE":
                print("ESP32 processed frame")
    except websockets.exceptions.ConnectionClosed:
        pass

async def send_screen(websocket):
    print("Connection is Successful! JPEG sending...")
    try:
        with mss.mss() as sct:
            monitor_to_capture = sct.monitors[MONITOR_NUMBER]
            print(f"Capture started: Display {MONITOR_NUMBER}")

            while True:
                start_time = asyncio.get_event_loop().time()
                sct_img = sct.grab(monitor_to_capture)
                resized_img = Image.frombytes("RGB", sct_img.size, sct_img.bgra, "raw", "BGRX").resize((320, 240), RESIZE_METHOD)
                
                buffer = io.BytesIO()
                resized_img.save(buffer, format="JPEG", quality=JPEG_QUALITY)
                jpeg_data = buffer.getvalue()

                await websocket.send(f"JPEG_FRAME_SIZE:{len(jpeg_data)}")
                for i in range(0, len(jpeg_data), CHUNK_SIZE):
                    chunk = jpeg_data[i:i + CHUNK_SIZE]
                    await websocket.send(chunk)

                elapsed_time = asyncio.get_event_loop().time() - start_time
                sleep_time = max(0, (1 / TARGET_FPS) - elapsed_time)
                await asyncio.sleep(sleep_time)
    except websockets.exceptions.ConnectionClosed:
        print("Screen share connection lost.")
        pass

async def main():
    uri = f"ws://{ESP32_IP}:{ESP32_PORT}"
    while True:
        try:
            print(f"Connecting to ESP32: {uri}")
            async with websockets.connect(uri, ping_interval=None, max_size=None, compression="deflate") as websocket:
                send_task = asyncio.create_task(send_screen(websocket))
                touch_task = asyncio.create_task(handle_touch(websocket))
                await asyncio.gather(send_task, touch_task)
        except (websockets.exceptions.ConnectionClosed, ConnectionRefusedError):
            print(f"Connection failed or lost. Retrying in 2 seconds...")
            await asyncio.sleep(2)
        except Exception as e:
            print(f"Unexpected Error: {e}")
            await asyncio.sleep(2)

if __name__ == "__main__":
    asyncio.run(main())
