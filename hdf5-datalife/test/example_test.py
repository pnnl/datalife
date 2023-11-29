import subprocess
import os

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
            # command = f"echo {task} > {vfd_task_file}"
            # subprocess.run(command, shell=True)
            with open(vfd_task_file, "w") as file:
                file.write(task)
            print(f"Overwrote: {vfd_task_file} with {task}")

    if vol_task_file and os.path.exists(vol_task_file):
        if os.path.isfile(vol_task_file):
            # command = f"echo {task} > {vol_task_file}"
            # subprocess.run(command, shell=True)
            with open(vol_task_file, "w") as file:
                file.write(task)
            print(f"Overwrote: {vol_task_file} with {task}")
    else:
        print("Invalid or missing PATH_FOR_TASK_FILES environment variable.")    

"""Write compressed chunks directly, bypassing HDF5's filters
"""
import h5py
import numpy as np
import zlib

set_curr_task("h5_write")

f = h5py.File("direct_chunk.h5", "w")

block_size = 2048
dataset = f.create_dataset(
    "data", (256, 1024, 1024), dtype="uint16", chunks=(64, 128, 128),
    compression="gzip", compression_opts=4,
)
# h5py's compression='gzip' is actually a misnomer: gzip does the same
# compression, but adds some extra metadata before & after the compressed data.
# This won't work if you use gzip.compress() instead of zlib!

# Random numbers with only a few possibilities, so some compression is possible.
array = np.random.randint(0, 10, size=(64, 128, 128), dtype=np.uint16)

# Compress the data, and write it into the dataset. (0, 0, 128) are coordinates
# for the start of a chunk. Equivalent to:
#   dataset[0:64, 0:128, 128:256] = array
compressed = zlib.compress(array, level=4)
dataset.id.write_direct_chunk((0, 0, 128), compressed)
print(f"Written {len(compressed)} bytes compressed data")

f.close()

f = h5py.File("direct_chunk.h5", "r")

# Read the chunk back (HDF5 will decompress it) and check the data is the same.
read_data = dataset[:64, :128, 128:256]
np.testing.assert_array_equal(read_data, array)
print(f"Verified array of {read_data.size} elements ({read_data.nbytes} bytes)")

f.close()