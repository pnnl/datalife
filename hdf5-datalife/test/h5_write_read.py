import h5py
import numpy as np
import os
import sys
import time
import subprocess

def set_curr_task(task):
    # os.environ['CURR_TASK'] = task
    # command = "echo running task [$CURR_TASK]"
    # command2 = "export CURR_TASK=$CURR_TASK"
    # subprocess.run(command, shell=True)
    # subprocess.run(command2, shell=True)
    
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
            command = f"echo {task} > {vfd_task_file}"
            subprocess.run(command, shell=True)
            # with open(vfd_task_file, "w") as file:
            #     file.write(task)
            # print(f"Overwrote: {vfd_task_file} with {task}")

    if vol_task_file and os.path.exists(vol_task_file):
        if os.path.isfile(vol_task_file):
            command = f"echo {task} > {vol_task_file}"
            subprocess.run(command, shell=True)
            # with open(vol_task_file, "w") as file:
            #     file.write(task)
            # print(f"Overwrote: {vol_task_file} with {task}")
    else:
        print("Invalid or missing PATH_FOR_TASK_FILES environment variable.")    


def set_curr_task_env(task):
    os.environ['CURR_TASK'] = task
    command = "echo running task [$CURR_TASK]"
    command2 = "export CURR_TASK=$CURR_TASK"
    subprocess.run(command, shell=True)
    subprocess.run(command2, shell=True)

def check_equals(datasets_1, datasets_2):
    for i in range(len(datasets_1)):
        if not np.array_equal(datasets_1[i], datasets_2[i]):
            print(f"Dataset {i}: Not Equal")
        else:
            print(f"Dataset {i}: Equal")

def run_write_read(file_name):
    # Create some sample data
    data1 = np.random.rand(100)
    data2 = np.random.randint(0, 10, size=100)
    data3 = np.random.randn(100)
    data4 = np.arange(100)
    
    set_curr_task_env("h5_write")
    # Create an HDF5 file and write datasets
    
    hdf_write = h5py.File(file_name, "w", libver="v112", swmr=False)

    hdf_write.create_dataset('dataset1', data=data1)
    hdf_write.create_dataset('dataset2', data=data2)
    hdf_write.create_dataset('dataset3', data=data3)
    hdf_write.create_dataset('dataset4', data=data4)
    
    hdf_write.flush()
    hdf_write.close()
    
    # sleep 3 seconds
    time.sleep(3)
    
    # dataset names
    dataset_names = ['dataset1', 'dataset2', 'dataset3', 'dataset4']
    
    set_curr_task_env("h5_read")
    
    print(f"Reading datasets {dataset_names} from file {file_name}")

    hdf_read = h5py.File(file_name, "r")
    # read datasets
    read_data1 = hdf_read[dataset_names[0]]
    read_data1 = np.array(read_data1)
    
    # Read datasets from the HDF5 file
    with h5py.File(file_name, 'r') as hdf_file:
        read_data1 = np.array(hdf_file['dataset1'])
        read_data2 = np.array(hdf_file['dataset2'])
        read_data3 = np.array(hdf_file['dataset3'])
        read_data4 = np.array(hdf_file['dataset4'])

    # # Check if the readback data equals the original data
    equal_data1 = np.array_equal(data1, read_data1)
    equal_data2 = np.array_equal(data2, read_data2)
    equal_data3 = np.array_equal(data3, read_data3)
    equal_data4 = np.array_equal(data4, read_data4)

    # # Print the comparison results
    print("Dataset 1: Equal =", equal_data1)
    print("Dataset 2: Equal =", equal_data2)
    print("Dataset 3: Equal =", equal_data3)
    print("Dataset 4: Equal =", equal_data4)

def run_write(file_name,datasets):
    task_name="h5_write"
    set_curr_task_env(task_name)
    print(f"Running Task : {task_name}")
    
    # with h5py.File(file_name, mode='w', swmr=False) as hdf_file:
    #     for i in range(len(datasets)):
    #         data=datasets[i]
    #         print(f"datatype is {data[0].dtype}")
    #         hdf_file.create_dataset(
    #             f'dataset{i}', 
    #             data=data,
    #             dtype=data[0].dtype)
    
    hf = h5py.File(file_name, mode='w')
    for i in range(len(datasets)):
        data=datasets[i]
        print(f"datatype is {data[0].dtype}")
        hf.create_dataset(
            f'dataset{i}', 
            data=data,
            dtype=data[0].dtype)
        # close dataset
        hf[f'dataset{i}'].id.close()
    hf.close()
    

def run_read(file_name, num_datasets):
    task_name = "h5_read"
    set_curr_task_env(task_name)
    print(f"Running Task : {task_name}")
    read_datasets = []
    
    with h5py.File(file_name, mode='r') as hdf_file:
        datasets = list(hdf_file.keys())
        print(f"Reading datasets {datasets} ")
        dataset_names = list(hdf_file.keys())
        for dataset in dataset_names:
            data = hdf_file[dataset][...]
            read_datasets.append(data)
    
    return read_datasets
    
def write_read_separate(file_name):
    dtype = np.uint16
    # Create some sample data
    d1 = np.random.randn(3, 10).astype(dtype)
    d2 = np.random.randn(3, 100).astype(dtype)
    d3 = np.random.randn(3, 1000).astype(dtype)

    # d1 = np.random.randint(0, 10, size=(64))
    # d2 = np.random.randint(0, 10, size=(64, 128))
    # d3 = np.random.randint(0, 10, size=(64, 128, 128))
    
    # d1 = d1.astype(int)
    # d2 = d2.astype(int)
    # d3 = d3.astype(int)
    
    # check all shaoes
    print(f"d1 shape {d1.shape}")
    print(f"d2 shape {d2.shape}")
    print(f"d3 shape {d3.shape}")
    
    write_datasets = [d1, d2, d3]
    
    run_write(file_name,write_datasets)
    read_datasets = run_read(file_name,len(write_datasets))
    
    if read_datasets:
        check_equals(write_datasets, read_datasets)

# main
if __name__ == "__main__":
    # get user argument with file name
    if len(sys.argv) < 2:
        print("Please provide a file name.")
        sys.exit(1)
    
    file_name = sys.argv[1]
    
    # run_write_read(file_name)
    write_read_separate(file_name)
