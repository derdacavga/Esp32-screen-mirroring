import asyncio
import websockets
import mss
from PIL import Image
import numpy as np
import io 

ESP32_IP = "192.168.1.112"
ESP32_PORT = 81
SCREEN_WIDTH = 320
SCREEN_HEIGHT = 240
JPEG_QUALITY = 40 
TARGET_FPS = 16

async def send_screen():
    uri = f"ws://{ESP32_IP}:{ESP32_PORT}"
    
    while True: 
        try:
            print(f"Connetting to ESP32: {uri}")
            async with websockets.connect(uri, ping_interval=None) as websocket:
                print("Connection Succes! sending Screen Image...")
                with mss.mss() as sct:
                    monitor = sct.monitors[1]
                    while True:
                        sct_img = sct.grab(monitor)
                        resized_img = Image.frombytes("RGB", sct_img.size, sct_img.bgra, "raw", "BGRX").resize((SCREEN_WIDTH, SCREEN_HEIGHT), Image.Resampling.LANCZOS)
                        
                        buffer = io.BytesIO()
                        resized_img.save(buffer, format="JPEG", quality=JPEG_QUALITY)
                        jpeg_data = buffer.getvalue()

                        await websocket.send(f"JPEG_FRAME_SIZE:{len(jpeg_data)}")
                        await websocket.send(jpeg_data)
                        await asyncio.sleep(1 / TARGET_FPS)

        except websockets.exceptions.ConnectionClosed as e:
            print(f"Connection Closed: {e}. Trying again...")
            await asyncio.sleep(2) 
        except Exception as e:
            print(f"Some error happen: {e}")
            await asyncio.sleep(2)

if __name__ == "__main__":
    asyncio.run(send_screen())