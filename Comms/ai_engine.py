# /home/xilinx/ai_engine.py
from pynq import Overlay, allocate
import numpy as np, logging

_ol = Overlay("/home/xilinx/bitstream/system_wrapper.xsa")
_ol.download()
_dma = _ol.axi_dma_0
_ip  = _ol.cnn_gd_0

_IN_SHAPE = (60, 6)     # yaw, pitch, roll, accelx, accely, accelz
_OUT_DIM  = 7

_CLASSES = ["Circle", "Infinity", "None", "Square", "Triangle", "Wave", "Zigzag"]
_CLASS_TO_LETTER = {
    0:'C', 1:'I', 2:'U', 3:'S', 4:'T', 5:'W', 6:'Z'
}

# --- Load global mean and std from file (generated in training) ---
_data = np.load("/home/xilinx/mean_std.npy", allow_pickle=True).item()
_channel_mean = _data["mean"].astype(np.float32)
_channel_std  = _data["std"].astype(np.float32)
_channel_std[_channel_std < 1e-6] = 1.0

# Allocate buffers
_inp = allocate(shape=(np.prod(_IN_SHAPE),), dtype=np.float32)
_out = allocate(shape=(_OUT_DIM,), dtype=np.float32)

def class_to_letter(idx: int) -> str:
    return _CLASS_TO_LETTER.get(int(idx), 'U')

def preprocess_window(window_60x6: np.ndarray) -> np.ndarray:
    """
    Apply global normalization as used during model training.
    - Data format: [yaw, pitch, roll, accelx, accely, accelz]
    - Orientation in degrees (raw values, not normalized)
    - Acceleration in g units (raw values, not normalized)
    """
    arr = window_60x6.astype(np.float32, copy=False)
    arr = (arr - _channel_mean) / _channel_std
    
    return arr

# Inference function (single call)
def infer(window_60x6: np.ndarray):
    """
    Run one-shot inference on a (60,6) IMU window.

    Args:
        window_60x6: np.ndarray of shape (60,6), float32

    Returns:
        (class_index:int, probs:np.ndarray[7], letter:str)
    """
    if window_60x6.shape != _IN_SHAPE:
        raise ValueError(f"Expected {_IN_SHAPE}, got {window_60x6.shape}")

    # Step 1: Preprocess
    mpu_data = preprocess_window(window_60x6)

    # Step 2: Load into DMA buffers
    flattened = mpu_data.flatten(order='C')  # Row-major (C-style) flattening
    _inp[:] = flattened
    _inp.flush()
    _out.fill(0)
    _out.invalidate()

    # Step 3: Kick DMA + IP
    _dma.recvchannel.transfer(_out)
    _dma.sendchannel.transfer(_inp)
    _ip.register_map.CTRL.AP_START = 1

    _dma.sendchannel.wait()
    _dma.recvchannel.wait()

    # Step 4: Read results (C++ already applies softmax, so output is already probabilities)
    _out.invalidate()
    probs = _out.copy()  # Already softmax probabilities from C++ model
    cls = int(np.argmax(probs))
    return cls, probs, class_to_letter(cls)
