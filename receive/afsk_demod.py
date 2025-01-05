#!/usr/bin/env python3
"""
afsk_demod.py

Continuously captures audio from a specified sound device and feeds it into
the ViperwolfFSKDecoder (CFFI-based extension). Decoded bits are ONLY turned
into a final ASCII message if:
  - We detect a PREAMBLE_BITS pattern, and
  - We detect an END_SEQ_BITS pattern within WAIT_FOR_END_SEC seconds
    from the last preamble detection.

If we detect a second preamble while in that wait window, we discard the old
partial bits and re-start the timer with the new preamble.

If no end-sequence is found by the timeout, the partial bits are discarded.

Press ENTER at any time to stop the script.

Dependencies:
    - sounddevice
    - numpy
    - viperwolf_wrapper (from viperwolf/python)
    - The compiled CFFI extension (_viperwolf_demod.so)

Author: Wayne Workman
Date: 2025-01-02
"""


import time
import threading
import datetime
import sounddevice as sd

# Import the custom wrapper that uses the Viperwolf CFFI extension
from viperwolf.python.viperwolf_wrapper import ViperwolfFSKDecoder

# -----------------------------
# GLOBAL CONSTANTS
# -----------------------------
AUDIO_DEVICE_ID     = 4            # <-- Change to your audio input device ID
AUDIO_GAIN          = 1.0          # <-- Adjust for audio amplitude scaling
DATA_LOG_FILE       = "afsk_decoded_messages.log"
DIAG_LOG_FILE       = "afsk_diagnostic.log"

CHUNK_SIZE          = 1024
SAMPLE_RATE         = 48000

PREAMBLE_BITS       = "101010101010"   # 12 bits: matches "PREAMBLE" in code.py
END_SEQ_BITS        = "11111111"       # 8 bits: matches "END_SEQUENCE" in code.py
WAIT_FOR_END_SEC    = 5.0             # how many seconds to wait after a preamble

SHOULD_EXIT         = False

# We only accumulate bits when "in_message" is True
# once we see a valid preamble, we set in_message = True, store bits,
# watch the clock, if we see end in time => we decode. If we see 2nd preamble => restart.
in_message          = False
preamble_detected_time = 0.0   # tracks when we last detected a preamble
accum_bits          = []       # bits from the last preamble until end or timeout

# -----------------------------
# LOGGING HELPER FUNCTIONS
# -----------------------------
def get_timestamp():
    """Return a string with the current local time. e.g. 2025-01-02 12:34:56."""
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def log_data_message(msg):
    """
    Append a line to the data log file, with a timestamp.
    Also prints to terminal.
    """
    timestr = get_timestamp()
    line = f"[{timestr}] {msg}\n"
    print(line, end="")
    try:
        with open(DATA_LOG_FILE, "a", encoding="utf-8") as f:
            f.write(line)
    except Exception as ex:
        log_diagnostic(f"Exception writing to data log file: {ex}")

def log_diagnostic(msg):
    """
    Append a line to the diagnostic log file, with a timestamp.
    Also prints to terminal.
    """
    timestr = get_timestamp()
    line = f"[{timestr}] DIAG: {msg}\n"
    print(line, end="")
    try:
        with open(DIAG_LOG_FILE, "a", encoding="utf-8") as f:
            f.write(line)
    except:
        pass

# -----------------------------
# DECODER SETUP
# -----------------------------
decoder = ViperwolfFSKDecoder(
    sample_rate=SAMPLE_RATE,
    baud_rate=300,
    mark_freq=1200,
    space_freq=2200
)

# -----------------------------
# BIT UTILS
# -----------------------------
def bits_to_ascii(bit_string):
    """
    Convert a raw bit string (e.g. '10101000') into ASCII in groups of 8 bits.
    Return the resulting string.
    """
    # Extra logging to see exactly what bits are being converted
    log_diagnostic(f"bits_to_ascii() called with bit_string={repr(bit_string)}")
    chars = []
    for i in range(0, len(bit_string), 8):
        chunk = bit_string[i : i+8]
        if len(chunk) < 8:
            log_diagnostic(f"bits_to_ascii() ignoring leftover bits: {chunk}")
            break  # ignore leftover
        val = 0
        for bit in chunk:
            val = (val << 1) | (1 if bit == '1' else 0)
        chars.append(chr(val))
    ascii_str = "".join(chars)
    log_diagnostic(f"bits_to_ascii() returning ASCII={repr(ascii_str)}")
    return ascii_str

# -----------------------------
# BIT-PROCESSING 
# -----------------------------
def handle_raw_bits(bit_list):
    """
    Accepts newly arrived bits from the ring buffer. 
    We'll only do something if we're in a "message" state or we see a preamble.
    """
    global in_message
    global accum_bits
    global preamble_detected_time

    if not bit_list:
        log_diagnostic("handle_raw_bits() called with an empty bit_list.")
    else:
        log_diagnostic(f"handle_raw_bits() got {len(bit_list)} new bits: {bit_list}")

    # 1) If we're currently ignoring bits (no preamble yet),
    #    see if the new chunk has a preamble.
    # 2) If we are collecting bits, see if we find a new preamble or an end sequence.

    new_str = "".join(str(b) for b in bit_list)
    log_diagnostic(f"Converted new bits to string: {new_str}")

    if in_message:
        # We are currently accumulating
        accum_bits.extend(bit_list)
        merged_str = "".join(str(b) for b in accum_bits)
        log_diagnostic(f"In-message mode. Merged bit string so far: {merged_str}")

        # see if there's an end sequence
        idx_end = merged_str.find(END_SEQ_BITS)
        if idx_end != -1:
            log_diagnostic(f"END_SEQ_BITS found at index={idx_end}")
            message_part = merged_str[:idx_end]
            ascii_text = bits_to_ascii(message_part)
            log_data_message(f"Complete message: {repr(ascii_text)}")

            in_message = False
            accum_bits = []
            return

        # else see if there's a second preamble that resets the timer
        idx_preamble_2 = merged_str.find(PREAMBLE_BITS)
        if idx_preamble_2 != -1:
            # We found a second preamble while still in the old message
            log_diagnostic(f"Second preamble found at index={idx_preamble_2}, discarding old partial message.")
            in_message = True
            preamble_detected_time = time.time()
            keep_index = idx_preamble_2 + len(PREAMBLE_BITS)
            leftover_str = merged_str[keep_index:]
            accum_bits = list(map(int, leftover_str))
            log_diagnostic(f"accum_bits replaced with leftover after second preamble: {accum_bits}")
            return

        # else check for timeout
        elapsed = time.time() - preamble_detected_time
        if elapsed > WAIT_FOR_END_SEC:
            log_diagnostic(f"No end-sequence found after {elapsed:.2f} sec; discarding partial bits.")
            in_message = False
            accum_bits = []
            return

    else:
        # we are ignoring bits => check if the new bits contain a preamble
        idx_preamble = new_str.find(PREAMBLE_BITS)
        if idx_preamble != -1:
            log_diagnostic(f"Preamble detected at index={idx_preamble}.")
            in_message = True
            preamble_detected_time = time.time()

            keep_index = idx_preamble + len(PREAMBLE_BITS)
            leftover_str = new_str[keep_index:]
            accum_bits = list(map(int, leftover_str))
            log_diagnostic(f"Started new message. accum_bits={accum_bits}")
        else:
            log_diagnostic("No preamble found; still ignoring bits.")

# -----------------------------
# THREAD FUNCTIONS
# -----------------------------
def audio_capture_loop():
    """
    Runs in a background thread. Captures audio in chunks from sounddevice,
    processes them with the ViperwolfFSKDecoder, and checks for decoded bits.
    """
    global SHOULD_EXIT

    try:
        with sd.InputStream(
            device=AUDIO_DEVICE_ID,
            samplerate=SAMPLE_RATE,
            channels=1,
            dtype='float32',
            blocksize=CHUNK_SIZE
        ) as stream:
            log_diagnostic("Audio input stream opened successfully.")
            
            while not SHOULD_EXIT:
                audio_chunk, overflowed = stream.read(CHUNK_SIZE)
                if overflowed:
                    log_diagnostic("Sounddevice reported an overflow.")

                audio_data = audio_chunk[:, 0].copy() * AUDIO_GAIN
                # Extra diag: log the first few samples
                log_diagnostic(f"Captured {len(audio_data)} samples. First 5 samples: {audio_data[:5].tolist()}")

                decoder.process_samples(audio_data)

                # retrieve any new bits
                raw_bits = decoder.get_raw_bits(4096)
                if raw_bits:
                    log_diagnostic(f"decoder.get_raw_bits() returned {len(raw_bits)} bits.")
                    handle_raw_bits(raw_bits)
                else:
                    log_diagnostic("decoder.get_raw_bits() returned 0 bits this chunk.")

                time.sleep(0.01)

    except Exception as e:
        log_diagnostic(f"Exception in audio_capture_loop: {e}")

    log_diagnostic("Audio capture loop exiting.")

def watch_for_enter_key():
    """
    This function blocks until user presses ENTER, then sets SHOULD_EXIT=True.
    """
    global SHOULD_EXIT
    input("\nPress ENTER at any time to stop...\n")
    SHOULD_EXIT = True

def main():
    log_diagnostic("Starting AFSK demod script.")
    
    capture_thread = threading.Thread(target=audio_capture_loop, daemon=True)
    capture_thread.start()

    watch_for_enter_key()

    log_diagnostic("Waiting for capture thread to terminate...")
    capture_thread.join()
    log_diagnostic("Capture thread terminated.")

    log_diagnostic("Exiting afsk_demod.py script.")

if __name__ == "__main__":
    main()

