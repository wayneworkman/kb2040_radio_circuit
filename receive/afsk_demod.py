#!/usr/bin/env python3
"""
afsk_demod.py

Continuously captures audio from a specified sound device and feeds it into
the ViperwolfFSKDecoder (CFFI-based extension). Decoded bits are accumulated,
converted into ASCII, and logged.

Additionally, we show how you might watch for a preamble or end-sequence,
though real AX.25 or other protocols require more robust parsing.

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
AUDIO_DEVICE_ID = 4            # <-- Change to your audio input device ID
AUDIO_GAIN      = 1.0          # <-- Adjust for audio amplitude scaling
DATA_LOG_FILE   = "afsk_decoded_messages.log"
DIAG_LOG_FILE   = "afsk_diagnostic.log"

CHUNK_SIZE      = 1024
SAMPLE_RATE     = 48000

# For demonstration, let's say we have a known preamble "10101010"
# and an end sequence "11111111" (very naive examples).
PREAMBLE_BITS   = "10101010"
END_SEQ_BITS    = "11111111"

SHOULD_EXIT     = False

# We'll accumulate raw bits in a buffer, convert to ASCII when we have 8 bits
accum_bits      = []
# We'll accumulate ASCII chars into a 'current message' until we detect an end sequence
current_message = []

# -----------------------------
# LOGGING HELPER FUNCTIONS
# -----------------------------
def get_timestamp():
    """
    Return a string with the current local time. e.g. 2025-01-02 12:34:56
    """
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
# BIT-PROCESSING / ASCII UTILS
# -----------------------------
def handle_raw_bits(bit_list):
    """
    Accepts a list of new bits (integers 0 or 1) from the ring buffer,
    merges them into a global 'accum_bits', and converts them to ASCII in groups of 8.
    Also does naive check for preamble and end-sequence.
    """
    global accum_bits
    global current_message

    # 1) Append the new bits to our global bit list
    accum_bits.extend(bit_list)

    # 2) Check each bit for (optional) naive preamble/end detection
    #    We'll do a "sliding window" approach on accum_bits, just to show the idea.
    #    This is very naive, doesn't handle transitions well. Real protocols are more complex.
    bit_string = "".join(str(b) for b in accum_bits)

    # If we find the preamble:
    idx_preamble = bit_string.find(PREAMBLE_BITS)
    if idx_preamble != -1:
        # We see a preamble. Possibly we "start a new message".
        log_diagnostic("Preamble detected! Starting a new message.")
        # Reset the current message buffer
        current_message = []
        # We'll just ignore bits up to that point, so let's re-slice accum_bits.
        keep_index = idx_preamble + len(PREAMBLE_BITS)
        accum_bits = list(map(int, list(bit_string[keep_index:])))
        return  # exit early, let new bits come in next iteration

    # If we find the end sequence:
    idx_end = bit_string.find(END_SEQ_BITS)
    if idx_end != -1:
        # We'll parse all bits up to that point into ASCII,
        # then treat that as a "complete message".
        # Then we remove them from accum_bits. The rest might be next message.
        message_part_bits = bit_string[:idx_end]  # bits up to end seq
        # convert that part to ASCII
        ascii_text = bits_to_ascii(message_part_bits)
        current_message.append(ascii_text)
        final_msg = "".join(current_message)

        # Log as a "complete" message
        log_data_message(f"Complete message: {repr(final_msg)}")

        # Now remove all bits including that end sequence
        keep_index = idx_end + len(END_SEQ_BITS)
        leftover = bit_string[keep_index:]
        accum_bits = list(map(int, list(leftover)))

        # Reset for next message:
        current_message = []
        return

    # 3) Otherwise, if no special sequences found, let's parse all we can.
    #    But let's not parse everything into ASCII yet if we might be waiting for a preamble, etc.
    #    For demonstration, we convert *all bits* in multiples of 8 to ASCII
    #    and append them to 'current_message'.
    partial_ascii, leftover_bits = convert_bits_in_chunks_of_8(accum_bits)
    if partial_ascii:
        # Accumulate these ASCII characters into current_message (not a "complete" message yet)
        current_message.append(partial_ascii)

    # leftover_bits are bits that don't make a full byte yet
    accum_bits = leftover_bits


def convert_bits_in_chunks_of_8(bit_list):
    """
    Given a list of bits (0/1), convert as many full bytes (8 bits) to ASCII as possible.
    Return (ascii_string, leftover_bits).
    ascii_string is the textual result. leftover_bits is a list of leftover bits < 8 in length.
    """
    result_chars = []
    length = len(bit_list)
    idx = 0
    while (idx + 7) < length:
        # collect 8 bits
        chunk = bit_list[idx : idx+8]
        idx += 8
        val = 0
        for bit in chunk:
            val = (val << 1) | bit
        # now 'val' is an integer from 0..255
        # convert to ASCII character
        c = chr(val)
        result_chars.append(c)

    leftover = bit_list[idx:]  # bits that didn't fit into a full byte
    return ("".join(result_chars), leftover)


def bits_to_ascii(bit_string):
    """
    Convert a raw bit string (e.g. '10101000') into ASCII in groups of 8 bits.
    Return the resulting string. 
    """
    chars = []
    # chunk the bit_string every 8 bits
    for i in range(0, len(bit_string), 8):
        chunk = bit_string[i : i+8]
        if len(chunk) < 8:
            break
        val = 0
        for bit in chunk:
            val = (val << 1) | (1 if bit == '1' else 0)
        chars.append(chr(val))
    return "".join(chars)


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

                # Flatten the single channel and apply gain
                audio_data = audio_chunk[:, 0].copy() * AUDIO_GAIN

                # Feed samples to the decoder
                decoder.process_samples(audio_data)

                # Then retrieve any raw bits
                raw_bits = decoder.get_raw_bits(4096)
                if raw_bits:
                    # Instead of printing them raw, let's handle them
                    handle_raw_bits(raw_bits)

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
