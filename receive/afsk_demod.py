from direwolf.python.fsk_wrapper import DirewolfFSKDecoder

decoder = DirewolfFSKDecoder(sample_rate=48000, baud_rate=300,
                             mark_freq=1200, space_freq=2200)

# Suppose we capture some audio chunk via PyAudio or sounddevice...
audio_chunk = ... # get from microphone, as numpy array float32

# Process:
decoder.process_samples(audio_chunk)

# Then get raw bits:
bits = decoder.get_raw_bits(4096)
print("Got bits:", bits)

