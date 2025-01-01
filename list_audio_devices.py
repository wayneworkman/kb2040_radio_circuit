#!/usr/bin/env python3

import sounddevice as sd

def main():
    print("Listing audio devices recognized by Python sounddevice:\n")
    devices = sd.query_devices()
    for i, dev in enumerate(devices):
        in_ch = dev["max_input_channels"]
        out_ch = dev["max_output_channels"]
        print(f"Device ID {i}: {dev['name']}")
        print(f"  Max input channels:  {in_ch}")
        print(f"  Max output channels: {out_ch}")
        print()

if __name__ == "__main__":
    main()
