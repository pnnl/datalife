import os
import random

def write_to_file(filename, chunk_size, total_size):
    data = b'a' * chunk_size
    with open(filename, 'wb') as f:
        for _ in range(total_size // chunk_size):
            f.write(data)

def read_from_file(filename, chunk_size):
    with open(filename, 'rb') as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            # Here we just read, but you can process the chunk if needed
            pass

def random_access_read(filename, chunk_size):
    with open(filename, 'rb') as f:
        # Get size of file
        fend = os.path.getsize(filename)
        
        # Ensure the file size is greater than or equal to the chunk size
        if fend < chunk_size:
            raise ValueError(f"File size ({fend}) is smaller than the chunk size ({chunk_size}).")
        
        # Get a random offset from 0 to fend - chunk_size
        read_cnt = 10
        for _ in range(read_cnt):
            offset = random.randint(0, fend - chunk_size)
            # read from offset
            f.seek(offset)
            chunk = f.read(chunk_size)
            # Process the chunk (if needed)
            # print(chunk)  # Uncomment to see the chunk data (for debugging purposes)




def main():
    filename = 'test_io_file.txt'
    chunk_size = 4 * 1024  # 16KB
    total_size = 64 * 1024 * 1024  # 64 MB

    print(f"Writing to {filename}...")
    write_to_file(filename, chunk_size, total_size)

    print(f"Reading from {filename}...")
    read_from_file(filename, chunk_size)
    # random_access_read(filename, chunk_size)

    print("Done.")

if __name__ == "__main__":
    main()