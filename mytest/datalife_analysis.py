import sys

def process_stat(task_stats):
    # Values order: time, cnt, amount(Bytes)
    # Calculate BW
    for task, occurrences in task_stats.items():
        for metric_type, metrics in occurrences.items():
            if not metrics:
                continue
            # Get key,value from metrics:
            for key, value_list in metrics.items():
                # convert to number
                _time = float(value_list[0])
                _cnt = int(value_list[1])
                _amount = int(value_list[2])
                if _cnt > 0 and _amount > 0 and _time != 0:
                    bw = _amount / _time
                    print(f"[{metric_type}]: key[{key}], time[{_time}], cnt[{_cnt}], amount[{_amount}], bw[{bw}]")
                # print(f"[{metric_type}]: key[{key}], time[{_time}], cnt[{_cnt}], amount[{_amount}]")

def print_stat(task_stats):
    # Print results
    for task, occurrences in task_stats.items():
        if occurrences:
            print(f"{task}:")
            for metric_type, metrics in occurrences.items():
                print(f"\'{metric_type}\': {metrics}")
        else:
            print(f"No occurrences of task '{task}' found in the log file.")
        print()

def get_stat_content(task_stats, task, stat_content):
    for line in stat_content:
        # Split line by space
        stat_content_list = line.split()
        metric_type = stat_content_list[1]
        metric_name = stat_content_list[2]
        metric_values = stat_content_list[3:]
        # print(f"metric_type[{metric_type}], metric_name[{metric_name}], values{metric_values}")
        task_stats[task][metric_type].update({metric_name: metric_values})

    # task_stats[task].append(stat_content)

def get_task_stats(log_file, tasks):

    # Check if log file exists
    try:
        with open(log_file, 'r') as file:
            log_lines = file.readlines()
    except FileNotFoundError:
        print(f"Error: Log file '{log_file}' not found.")
        sys.exit(1)

    # Dictionary to store found tasks
    task_stats = {}
    # Initialize the dictionary with 3 metric types
    metric_types = ['tazer', 'local', 'system']
    for task in tasks:
        task_stats[task] = {}
        for m in metric_types:
            task_stats[task][m] = {}


    # Read the log file line by line and look for task names
    i = 0
    while i < len(log_lines):
        line = log_lines[i]
        for task in tasks:
            task_str = f"[TAZER] {task}"
            if task_str in line:
                # Store the rest of stat content into a list of lines until it reaches an empty line
                stat_content = []
                i += 1
                while i < len(log_lines) and log_lines[i].strip():
                    stat_content.append(log_lines[i].strip())
                    i += 1
                get_stat_content(task_stats, task, stat_content)
        i += 1

    #print_stat(task_stats)
    process_stat(task_stats)


def main():
    # Take first argument as the log file we are using
    log_file = sys.argv[1]
    # Take the rest of the arguments as the task names we are looking for
    tasks = sys.argv[2:]
    
    print(f"Getting stats from [{log_file}] with tasks {tasks}")

    get_task_stats(log_file,tasks)



if __name__ == "__main__":
    main()
