#!/usr/bin/env python3

"""
afsk_demod.py
A Python script for real-time AFSK demodulation using multiprocessing and sounddevice.
Captures audio from a Linux audio input device at 48 kHz, detects transmissions with a known
preamble and end sequence, decodes the bits, and logs the results.

Author: Wayne Workman
"""

import sys
import time
import queue
import logging
import datetime
from multiprocessing import Process, Queue

import sounddevice as sd
import numpy as np

# -------------------------
# GLOBAL CONSTANTS
# -------------------------

# FSK parameters
FREQ0 = 1200          # Lower frequency (Mark)
FREQ1 = 2200          # Higher frequency (Space)
BAUD_RATE = 300      # Bits per second for actual transmissions
SAMPLE_RATE = 48000   # Audio sample rate in Hz (48 kHz to avoid ALSA resampling)

# Protocol constants
PREAMBLE_BITS  = "101010101010"  # Preamble bit-pattern to detect
END_SEQUENCE   = "11111111"      # End bit-pattern to detect

# Logging / operational constants
AUDIO_DEVICE = None       # None = default input device, or specify name/index
AUDIO_GAIN = 1.0          # Multiply raw samples by this to adjust amplitude
MAX_BUFFER_SECONDS = 5    # If preamble found but no end sequence in 5s, reset

# Multiprocessing queues size constraints
AUDIO_QUEUE_MAXSIZE = 10   # Number of audio chunks to store in memory
MESSAGE_QUEUE_MAXSIZE = 10 # Decoded messages queue

# Diagnostic logging verbosity
DIAGNOSTIC_LOGGING = True

# Print all log messages (data + diagnostic) to terminal?
PRINT_TO_TERMINAL = True

# Logging paths
DATA_LOG_FILE       = "afsk_data.log"
DIAGNOSTIC_LOG_FILE = "afsk_diagnostic.log"

# -------------------------
# LOGGING SETUP
# -------------------------

data_logger = logging.getLogger("data_logger")
diagnostic_logger = logging.getLogger("diagnostic_logger")

formatter = logging.Formatter("[%(asctime)s] %(levelname)s: %(message)s")

# Data log file handler
data_handler = logging.FileHandler(DATA_LOG_FILE, mode='a')
data_handler.setFormatter(formatter)
data_logger.setLevel(logging.INFO)
data_logger.addHandler(data_handler)

# Diagnostic log file handler
diag_handler = logging.FileHandler(DIAGNOSTIC_LOG_FILE, mode='a')
diag_handler.setFormatter(formatter)
diagnostic_logger.setLevel(logging.DEBUG if DIAGNOSTIC_LOGGING else logging.INFO)
diagnostic_logger.addHandler(diag_handler)

# If PRINT_TO_TERMINAL is True, we add console handlers for both loggers
if PRINT_TO_TERMINAL:
    console_handler_data = logging.StreamHandler(sys.stdout)
    console_handler_data.setFormatter(formatter)
    data_logger.addHandler(console_handler_data)
    
    console_handler_diag = logging.StreamHandler(sys.stdout)
    console_handler_diag.setFormatter(formatter)
    diagnostic_logger.addHandler(console_handler_diag)

# -------------------------
# HELPER FUNCTIONS
# -------------------------

def now_str():
    """Returns current local time as a string (YYYY-MM-DD HH:MM:SS)."""
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def goertzel(samples, freq, sample_rate):
    """
    Goertzel filter to detect energy at 'freq' in 'samples'.
    Returns the squared magnitude of that frequency component.
    """
    N = len(samples)
    k = int(round(0.5 + (N * freq / sample_rate)))
    omega = (2.0 * np.pi * k) / N
    sin_ = np.sin(omega)
    cos_ = np.cos(omega)
    coeff = 2.0 * cos_

    s_prev = 0.0
    s_prev2 = 0.0

    for sample in samples:
        s = sample + coeff * s_prev - s_prev2
        s_prev2 = s_prev
        s_prev = s

    power = s_prev2*s_prev2 + s_prev*s_prev - coeff*s_prev*s_prev2
    return power

def bit_from_chunk(chunk, bit_index=0):
    """
    Given a chunk of audio samples (covering one bit duration),
    use Goertzel to decide which tone is present (FREQ0 or FREQ1).
    Returns '0' or '1'.

    Added extensive diagnostic logging here:
      - peak_amplitude
      - p0, p1, ratio p0/p1
      - chosen bit
    """
    # Convert chunk to float32
    samples = np.array(chunk, dtype=np.float32) * AUDIO_GAIN

    # Basic amplitude stats
    peak_amplitude = np.max(np.abs(samples))
    rms_amplitude = np.sqrt(np.mean(samples**2))

    p0 = goertzel(samples, FREQ0, SAMPLE_RATE)
    p1 = goertzel(samples, FREQ1, SAMPLE_RATE)

    # Avoid division by zero
    ratio = p0 / (p1 + 1e-9)

    chosen_bit = '0' if p0 > p1 else '1'

    # Extra diagnostic for each bit
    if DIAGNOSTIC_LOGGING:
        diagnostic_logger.debug(
            f"Bit #{bit_index}: peak={peak_amplitude:.4f}, rms={rms_amplitude:.4f}, "
            f"p0={p0:.2f}, p1={p1:.2f}, ratio={ratio:.3f}, chosen='{chosen_bit}'"
        )

    return chosen_bit

# -------------------------
# DEMODULATOR PROCESS
# -------------------------

def demodulator_process(audio_q, message_q):
    """
    Consumes audio from audio_q, processes it in bit-sized chunks,
    detects preamble -> message -> end sequence, and pushes
    decoded messages into message_q.
    """
    samples_per_bit = int(SAMPLE_RATE // BAUD_RATE)
    leftover_buffer = np.array([], dtype=np.float32)

    searching_preamble = True
    found_preamble_time = None
    bit_buffer = ""
    total_bit_count = 0  # We'll track how many bits we've processed for debugging

    preamble_len = len(PREAMBLE_BITS)
    endseq_len   = len(END_SEQUENCE)

    if DIAGNOSTIC_LOGGING:
        diagnostic_logger.info("Demodulator process started, entering loop.")

    while True:
        try:
            chunk = audio_q.get(timeout=1.0)  # Wait up to 1 second for new audio
        except queue.Empty:
            # No audio arrived in 1 second
            if DIAGNOSTIC_LOGGING:
                diagnostic_logger.debug("demodulator_process: audio_q is empty, continuing...")
            continue

        # Convert chunk to float32 array
        chunk_np = np.array(chunk, dtype=np.float32)
        leftover_buffer = np.concatenate((leftover_buffer, chunk_np))

        if DIAGNOSTIC_LOGGING:
            diagnostic_logger.debug(
                f"Received {len(chunk_np)} samples from audio_q, leftover_buffer size={len(leftover_buffer)}"
            )

        # Process as many bits as possible
        while len(leftover_buffer) >= samples_per_bit:
            bit_index = total_bit_count
            bit_samples = leftover_buffer[:samples_per_bit]
            leftover_buffer = leftover_buffer[samples_per_bit:]

            detected_bit = bit_from_chunk(bit_samples, bit_index=bit_index)
            bit_buffer += detected_bit
            total_bit_count += 1

            if searching_preamble:
                # Check if last N bits match PREAMBLE_BITS
                if len(bit_buffer) >= preamble_len:
                    tail = bit_buffer[-preamble_len:]
                    if tail == PREAMBLE_BITS:
                        searching_preamble = False
                        found_preamble_time = time.time()
                        # Keep only the preamble in the buffer
                        bit_buffer = PREAMBLE_BITS
                        diagnostic_logger.info(
                            f"Preamble detected at {now_str()} (bit #{bit_index})"
                        )
            else:
                # Already found preamble, look for end sequence
                if len(bit_buffer) >= preamble_len + endseq_len:
                    tail = bit_buffer[-endseq_len:]
                    if tail == END_SEQUENCE:
                        # Full transmission found
                        message_bits = bit_buffer[preamble_len:-endseq_len]
                        searching_preamble = True
                        bit_buffer = ""
                        found_preamble_time = None

                        decoded_message = bits_to_string(message_bits, msb_first=True)
                        message_q.put(decoded_message)

                        diagnostic_logger.info(
                            f"End sequence found at bit #{bit_index}. Message: '{decoded_message}'"
                        )
                    else:
                        # Check time limit
                        if found_preamble_time and (time.time() - found_preamble_time) > MAX_BUFFER_SECONDS:
                            # Timed out waiting for end sequence
                            searching_preamble = True
                            bit_buffer = ""
                            found_preamble_time = None
                            diagnostic_logger.warning(
                                "Preamble found but end sequence not detected within time limit. Resetting."
                            )
                else:
                    # Not enough bits for end seq. Check time limit
                    if found_preamble_time and (time.time() - found_preamble_time) > MAX_BUFFER_SECONDS:
                        searching_preamble = True
                        bit_buffer = ""
                        found_preamble_time = None
                        diagnostic_logger.warning(
                            "Preamble found but end sequence not detected within time limit. Resetting."
                        )

def bits_to_string(bit_str, msb_first=True):
    """
    Convert a string of '0'/'1' bits into ASCII text, 8 bits per character.
    If msb_first=True, bit_str[0] is MSB of the first byte.
    """
    length = len(bit_str) - (len(bit_str) % 8)
    bit_str = bit_str[:length]

    chars = []
    for i in range(0, length, 8):
        byte_bits = bit_str[i:i+8]
        if msb_first:
            val = int(byte_bits, 2)
        else:
            val = int(byte_bits[::-1], 2)
        chars.append(chr(val))
    return "".join(chars)


# -------------------------
# AUDIO CAPTURE PROCESS
# -------------------------

def audio_capture_process(audio_q):
    """
    Captures audio at SAMPLE_RATE from the specified AUDIO_DEVICE, splits it into 
    small chunks, and pushes those chunks into audio_q.
    """
    chunk_frames = 256  # frames per audio chunk

    def audio_callback(indata, frames, time_info, status):
        if status:
            diagnostic_logger.warning(f"Sounddevice status: {status}")
        # If multi-channel, take only first channel
        mono_data = indata[:, 0]
        try:
            audio_q.put_nowait(mono_data)
        except queue.Full:
            if DIAGNOSTIC_LOGGING:
                diagnostic_logger.warning("Audio queue is full. Dropping chunk.")

    diagnostic_logger.info("Audio capture process running at 48 kHz...")

    with sd.InputStream(samplerate=SAMPLE_RATE,
                        blocksize=chunk_frames,
                        device=AUDIO_DEVICE,
                        channels=1,
                        callback=audio_callback):
        while True:
            time.sleep(0.2)


# -------------------------
# MAIN PROCESS
# -------------------------

def main():
    diagnostic_logger.info("Starting AFSK demodulator script with 48 kHz sample rate...")

    audio_q   = Queue(maxsize=AUDIO_QUEUE_MAXSIZE)
    message_q = Queue(maxsize=MESSAGE_QUEUE_MAXSIZE)

    p_audio = Process(target=audio_capture_process, args=(audio_q,), daemon=True)
    p_demod = Process(target=demodulator_process, args=(audio_q, message_q), daemon=True)

    p_audio.start()
    p_demod.start()

    diagnostic_logger.info("Processes started. Listening for transmissions...")

    try:
        while True:
            try:
                message = message_q.get(timeout=1.0)
                # Log the received message
                timestamp = now_str()
                data_logger.info(f"[{timestamp}] RX MESSAGE: {message}")
                diagnostic_logger.info(f"Logged message: '{message}' at {timestamp}")
            except queue.Empty:
                pass
    except KeyboardInterrupt:
        diagnostic_logger.info("Keyboard interrupt detected. Shutting down.")
    finally:
        p_audio.terminate()
        p_demod.terminate()
        p_audio.join()
        p_demod.join()
        diagnostic_logger.info("AFSK demodulator script exited.")


if __name__ == "__main__":
    main()
