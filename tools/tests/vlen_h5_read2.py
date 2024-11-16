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

def run_read(file_name):
    task_name = "h5_read2"
    print(f"Running Task: {task_name}")
    set_curr_task(task_name)

    hf = h5py.File(file_name, 'r')
    group = hf['data']
    datasets = []
    for key in group.keys():
        ds = group[key]

        # Check if the dataset contains variable-length sequences
        if ds.dtype.kind == 'O' and ds.shape[1:] == ():
            # If yes, convert to a list of NumPy arrays
            data = [np.array(item, dtype=np.float32) for item in ds[:]]
            
        else:
            # Otherwise, convert the whole dataset to a NumPy array
            data = np.array(ds[:], dtype=np.float32)
        
        # add 1 to each element
        data = [item + 1 for item in data]
        
        # # print each item in data
        # print(f"Data [{key}]:")
        # for item in data:
        #     print(item)

        print(f"Read dataset {key} with datatype {ds.dtype}")
    
    hf.close()    



if __name__ == "__main__":
    # get user argument with file name
    if len(sys.argv) < 2:
        print("Please provide a file name.")
        sys.exit(1)

    file_name = sys.argv[1]
    
    print(f"Running test for file {file_name}")
    run_read(file_name)


