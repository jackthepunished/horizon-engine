import wave
import math
import struct

def generate_beep(filename, duration=0.2, frequency=440.0):
    sample_rate = 44100
    n_samples = int(sample_rate * duration)
    
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(sample_rate)
        
        for i in range(n_samples):
            # Simple sine wave with decay
            t = float(i) / sample_rate
            decay = max(0, 1.0 - t / duration)
            value = 32767.0 * decay * math.sin(2.0 * math.pi * frequency * t)
            data = struct.pack('<h', int(value))
            wav_file.writeframesraw(data)

if __name__ == "__main__":
    print("Generating assets/audio/impact.wav...")
    generate_beep("assets/audio/impact.wav", duration=0.3, frequency=150.0)
    print("Done.")
