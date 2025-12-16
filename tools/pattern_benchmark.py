import random

def run_benchmark():
    fps = random.randint(110, 200)  # Replace with real measurement
    if fps < 120:
        print(f"FAIL: FPS too low ({fps})")
        exit(1)
    print(f"PASS: FPS {fps}")
    exit(0)

if __name__ == "__main__":
    run_benchmark() 