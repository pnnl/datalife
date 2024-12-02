import h5py
import numpy as np
import os
import sys
import subprocess

def run_read(file_name):
    task_name = "h5_read2"
    print(f"Running Task: {task_name}")

    # Open the HDF5 file in read mode
    hf = h5py.File(file_name, 'r')
    
    # Iterate over datasets at the root level
    for key in hf.keys():
        ds = hf[key]

        # Check if the dataset contains variable-length sequences
        if ds.dtype.kind == 'O' and ds.shape[1:] == ():
            # If yes, convert to a list of NumPy arrays
            data = [np.array(item, dtype=np.float32) for item in ds[:]]
        else:
            # Otherwise, convert the whole dataset to a NumPy array
            data = np.array(ds[:], dtype=np.float32)
        
        # Add 1 to each element in the dataset
        data = data + 1
        
        # Print dataset details
        print(f"Read dataset {key} with shape {ds.shape} and datatype {ds.dtype}")

    hf.close()


if __name__ == "__main__":
    # Get user argument with file name
    if len(sys.argv) < 2:
        print("Please provide a file name.")
        sys.exit(1)

    file_name = sys.argv[1]
    print(f"Running test for file {file_name}")
    run_read(file_name)