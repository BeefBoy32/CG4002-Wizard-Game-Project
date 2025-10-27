#!/usr/bin/env python3
"""
Run gesture classification on Ultra96 FPGA with AXI DMA.
"""

from pynq import Overlay, allocate
import logging
import time
import numpy as np
from collections import deque

# 1. Setup logger 
logger = logging.getLogger("ai_engine")
logger.setLevel(logging.INFO)

ch = logging.StreamHandler()
ch.setLevel(logging.INFO)
formatter = logging.Formatter(
    "%(asctime)s - %(levelname)s - %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
ch.setFormatter(formatter)
logger.addHandler(ch)

# 2. Load overlay 
ol = Overlay("/home/xilinx/bitstream/system_wrapper.xsa")
ol.download()
logger.info("Bitstream loaded successfully.")

# 3. Access IP and DMA 
dma = ol.axi_dma_0
ip = ol.cnn_gd_0

logger.info(f"DMA:{dma}")
logger.info(f"IP Block:{ip}")

# 4. Define gesture classes 
wand_classes = ["Circle", "Infinity", "None", "Square", "Triangle", "Wave", "Zigzag"]

# 5. Prepare input data 
'''
# Simulated queue of dicts (replace with your actual source)
sensor_queue = [
    {"ts": i, "yaw": 0.5*i, "pitch": 0.2*i, "roll": 0.1*i,
     "accelx": 100+i, "accely": -50+i, "accelz": 980}
    for i in range(60)
]
'''
mpu_data = np.random.randn(60, 6).astype(np.float32)  # Dummy data
'''
sensor_queue = deque(maxlen=60)

def add_sample(sample_dict):
    sensor_queue.append(sample_dict)

if len(sensor_queue) == 60:
mpu_data = np.array([  
    [
        s["yaw"],
        s["pitch"],
        s["roll"],
        float(s["accelx"]),
        float(s["accely"]),
        float(s["accelz"])
    ]
    for s in sensor_queue
], dtype=np.float32)  
'''
logger.info("Input data received.")

if mpu_data.shape != (60, 6):
    raise ValueError(f"MPU data must be shape (60, 6). Got: {mpu_data.shape}")

N = 60 * 6   # 360 inputs
M = 7        # 7 output classes

# -------------------- 6. DMA Buffer Preparation --------------------
logger.info("Allocating DMA buffers...")
inp_buffer = allocate(shape=(N,), dtype=np.float32)
out_buffer = allocate(shape=(M,), dtype=np.float32)

inp_buffer[:] = mpu_data.flatten()
out_buffer.fill(0)

inp_buffer.flush()
out_buffer.invalidate()

# -------------------- 7. Run inference --------------------
logger.info("Starting DMA transfer and inference...")
start = time.time()

dma.recvchannel.transfer(out_buffer)
dma.sendchannel.transfer(inp_buffer)

ip.register_map.CTRL.AUTO_RESTART = 0
ip.register_map.CTRL.AP_START = 1

dma.sendchannel.wait()
logger.info("DMA sendchannel transfer complete.")
dma.recvchannel.wait()
logger.info("DMA recvchannel transfer complete.")

elapsed = time.time() - start
logger.info(f"Inference completed in {elapsed*1000:.3f} ms")

# -------------------- 8. Post-processing --------------------
out_buffer.invalidate()

raw_output = np.array(out_buffer, copy=True)
logger.info(f"Raw AI output values: {raw_output}")

def softmax(x):
    x = x - np.max(x)
    e = np.exp(x)
    return e / np.sum(e)

probs = softmax(raw_output)
predicted_idx = int(np.argmax(probs))
predicted_symbol = wand_classes[predicted_idx]

# -------------------- 9. Results --------------------
logger.info("Printing input MPU data:")
for i, row in enumerate(mpu_data):
    logger.info(
        f"Timestep {i+1:02d} | "
        f"Yaw: {row[0]:.2f}, Pitch: {row[1]:.2f}, Roll: {row[2]:.2f}, "
        f"AccelX: {row[3]:.2f}, AccelY: {row[4]:.2f}, AccelZ: {row[5]:.2f}"
    )

logger.info(f"Predicted Gesture: {predicted_symbol}")
logger.info(f"Probabilities: {probs}")
