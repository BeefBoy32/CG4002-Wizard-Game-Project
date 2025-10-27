from pynq import Overlay, allocate
import numpy as np, time, logging
from collections import deque

logger = logging.getLogger("ai_engine")
logger.setLevel(logging.INFO)
logger.addHandler(logging.StreamHandler())

# Load overlay
ol = Overlay("/home/xilinx/bitstream/system_wrapper.xsa")
ol.download()
logger.info("Overlay loaded")

dma = ol.axi_dma_0
ip = ol.cnn_gd_0
logger.info(f"DMA:{dma}")
logger.info(f"IP Block:{ip}")

classes = ["Circle", "Infinity", "None", "Square", "Triangle", "Wave", "Zigzag"]

inp_buffer = allocate(shape=(60*6,), dtype=np.float32)
out_buffer = allocate(shape=(7,), dtype=np.float32)
sensor_queue = deque(maxlen=60)

while True:
    # Simulate incoming data
    sensor_queue.append({
        "ts": time.time(),
        "yaw": np.random.uniform(-180, 180),
        "pitch": np.random.uniform(-90, 90),
        "roll": np.random.uniform(-180, 180),
        "accelx": np.random.randint(-16384, 16384),
        "accely": np.random.randint(-16384, 16384),
        "accelz": np.random.randint(-16384, 16384)
    })

    if len(sensor_queue) < 60:
        continue  # wait until window fills

    # Convert deque → numpy array
    mpu_data = np.array([
        [s["yaw"], s["pitch"], s["roll"], float(s["accelx"]), float(s["accely"]), float(s["accelz"])]
        for s in sensor_queue
    ], dtype=np.float32)

    # Step 1: scale raw IMU values to training ranges
    mpu_data[:, :3] /= 180.0     # yaw, pitch, roll
    mpu_data[:, 3:] /= 16384.0   # accelx, accely, accelz

    # Step 2: per-window standardization (same as StandardScaler().fit_transform)
    mean = np.mean(mpu_data, axis=0, keepdims=True)
    std = np.std(mpu_data, axis=0, keepdims=True)
    std[std < 1e-6] = 1e-6  # avoid divide-by-zero
    mpu_data = (mpu_data - mean) / std

    # Optional Step 3: clip extreme values (for safety)
    np.clip(mpu_data, -4.0, 4.0, out=mpu_data)

    # Step 4: flatten and send to FPGA
    inp_buffer[:] = mpu_data.flatten()
    inp_buffer.flush()
    out_buffer.fill(0)
    out_buffer.invalidate()

    dma.recvchannel.transfer(out_buffer)
    dma.sendchannel.transfer(inp_buffer)
    ip.register_map.CTRL.AP_START = 1

    dma.sendchannel.wait()
    dma.recvchannel.wait()

    # Step 5: compute softmax + display prediction
    out_buffer.invalidate()
    probs = np.exp(out_buffer - np.max(out_buffer))
    probs /= np.sum(probs)
    pred = np.argmax(probs)

    logger.info(f"Gesture: {classes[pred]} | probs={probs}")
    time.sleep(0.5)
