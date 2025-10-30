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

# Allocate buffers
_inp = allocate(shape=(np.prod(_IN_SHAPE),), dtype=np.float32)
_out = allocate(shape=(_OUT_DIM,), dtype=np.float32)

def class_to_letter(idx: int) -> str:
    return _CLASS_TO_LETTER.get(int(idx), 'U')

def _softmax(x: np.ndarray) -> np.ndarray:
    x = x - np.max(x)
    e = np.exp(x, dtype=np.float32)
    return e / np.sum(e)

def preprocess_window(window_60x6: np.ndarray) -> np.ndarray:
    """
    Apply IMU preprocessing as used during model training.
    - Normalize yaw/pitch/roll to [-1, 1]
    - Normalize accelerometer values
    - Per-window standardization (StandardScaler)
    - Clip extreme values
    """
    arr = window_60x6.astype(np.float32, copy=False)
    arr[:, :3] /= 180.0      # yaw, pitch, roll
    arr[:, 3:] /= 16384.0    # accelx, accely, accelz

    mean = np.mean(arr, axis=0, keepdims=True)
    std = np.std(arr, axis=0, keepdims=True)
    std[std < 1e-6] = 1e-6
    arr = (arr - mean) / std

    np.clip(arr, -4.0, 4.0, out=arr)
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
    _inp[:] = mpu_data.flatten()
    _inp.flush()
    _out.fill(0)
    _out.invalidate()

    # Step 3: Kick DMA + IP
    _dma.recvchannel.transfer(_out)
    _dma.sendchannel.transfer(_inp)
    _ip.register_map.CTRL.AP_START = 1

    _dma.sendchannel.wait()
    _dma.recvchannel.wait()

    # Step 4: Compute softmax + return results
    _out.invalidate()
    probs = _softmax(_out)
    cls = int(np.argmax(probs))
    return cls, probs, class_to_letter(cls)
