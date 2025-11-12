import h5py
import numpy as np
import os
import sys
import time
import subprocess

def set_curr_task(task):
    
    workflow_name = os.environ.get("WORKFLOW_NAME")
    path_for_task_files = os.environ.get("PATH_FOR_TASK_FILES")
    vfd_task_file = None
    vol_task_file = None
    
    if workflow_name and path_for_task_files:
        vfd_task_file = os.path.join(path_for_task_files, f"{workflow_name}_vfd.curr_task")
        vol_task_file = os.path.join(path_for_task_files, f"{workflow_name}_vol.curr_task")

    # vfd_task_file = /tmp/$USER/pyflextrkr_vfd.curr_task
    
    if vfd_task_file and os.path.exists(vfd_task_file):
        if os.path.isfile(vfd_task_file):
            command = f"echo -n {task} > {vfd_task_file}"
            subprocess.run(command, shell=True)


    if vol_task_file and os.path.exists(vol_task_file):
        if os.path.isfile(vol_task_file):
            command = f"echo -n {task} > {vol_task_file}"
            subprocess.run(command, shell=True)

    else:
        print("Invalid or missing PATH_FOR_TASK_FILES environment variable.")    


def set_curr_task_env(task):
    os.environ['CURR_TASK'] = task
    command = "echo running task [$CURR_TASK]"
    command2 = "export CURR_TASK=$CURR_TASK"
    subprocess.run(command, shell=True)
    subprocess.run(command2, shell=True)

def check_equals(datasets_1, datasets_2):
    print("Checking if datasets are equal")
    for i in range(len(datasets_1)):
        if not np.array_equal(datasets_1[i], datasets_2[i]):
            print(f"Dataset {i}: Not Equal")
        else:
            print(f"Dataset {i}: Equal")

def run_write(file_name, write_datasets, dtype):
    try:
        # Get directory from file path
        file_dir = os.path.dirname(file_name)
        
        # Ensure the directory exists
        if file_dir and not os.path.exists(file_dir):
            os.makedirs(file_dir, exist_ok=True)
            print(f"Created directory: {file_dir}")

        # Open the file for writing
        with h5py.File(file_name, mode='w') as hf:
            for dataset_name, data in write_datasets.items():
                hf.create_dataset(dataset_name, data=data, dtype=dtype)
            print(f"Successfully wrote data to {file_name}")

    except FileNotFoundError as e:
        print(f"Error: {e}")
        print("Check if the file path is correct.")
    except OSError as e:
        print(f"Error: {e}")
        print("Check if the directory is writable.")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":
    # Get user argument with file name
    if len(sys.argv) < 2:
        print("Please provide a file name.")
        sys.exit(1)

    file_name = sys.argv[1]
    
    print(f"Writing to file {file_name}")

    # Specify dtype
    dtype = 'float32'

    # Create some sample data
    d1 = np.random.randn(100).astype(dtype)
    d2 = np.random.randn(1000).astype(dtype)
    d3 = np.random.randn(10000).astype(dtype)
    d4 = np.random.randn(300, 1000).astype(dtype)

    # Check all shapes
    print(f"d1 shape {d1.shape}")
    print(f"d2 shape {d2.shape}")
    print(f"d3 shape {d3.shape}")
    print(f"d4 shape {d4.shape}")

    # Create a dictionary for datasets
    write_datasets = {
        "dataset1": d1,
        "dataset2": d2,
        "dataset3": d3,
        "dataset4": d4
    }

    run_write(file_name, write_datasets, dtype)


