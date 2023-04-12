# generate report for sequential I/O access pattern

from audioop import add, mul
from cmath import inf
from itertools import count
import pandas as pd
import sys
import numpy as np
from matplotlib import pyplot as plt
import re
import os

ko = 'OPEN'
kc = 'CLOSE'
kr = 'READ'
kw = 'WRITE'

ignore_line = ['Dataset_Dimension']

datasets = ['contact_map', 'point_cloud']

io_size_key = []

def humansize(nbytes):
    suffixes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB']
    i = 0
    while nbytes >= 1000 and i < len(suffixes)-1:
        nbytes /= 1000.
        i += 1
    f = ('%.2f' % nbytes).rstrip('0').rstrip('.')
    return '%s %s' % (f, suffixes[i])

def plot_hist(s):
    x = np.array(s)
    # x = np.log2(x)

    # Creating histogram
    plt.hist(x, density=True, bins=30)  # density=False would make counts
    plt.ylabel('Probability')
    plt.xlabel('Data Access Size')
    # Show plot
    plt.show()

def plot_access(df,infile):
    plt.figure(figsize=(40,6))
    x = df['Order']
    y = df['Access_Size']
    y[y<0]=0
    y = np.log2(y)
    # y = y/4096
    plt.bar(x,y)
    # plt.show()
    plt.xlabel('Access Order')
    plt.ylabel('Access Size in Bytes Log Scale')
    
    figname = infile[26:-9]
    plt.title(f'Data Access Size Plot in Recorded Order')
    figname += '-acc_size_seq.'
    print(figname)
    plt.savefig(f"graphs/{figname}")

def agg_frame_report(df):
    pass

def sim_add_row(type,case,df,count,filename,size=''):
    if case == 0:
        row = [type,'~ 37KB',count,filename]
    elif case == 1:
        row = [type,size,count,filename]
    elif case == 2:
        row = [type,'-1',count,filename]
    elif case == 3:
        row = [type,'-1',count,filename]
    else:
        row = [type,'Meta',count,filename]

    df.loc[len(df.index)] = row

def sim_frame_report(df):
    access_record = pd.DataFrame(columns = ["Type", "Access","Count","Filename"])
    count = 0
    case = 0
    type = ''
    husize = ''
    filename = ''
    
    for index, row in df.iterrows():
        if row['Access_Region_Mem_Type'] == 'H5FD_MEM_DRAW':
            if row['Access_Size'] < 38000 and row['Access_Size'] > 36500: # case 0
                if case !=0 : 
                    sim_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 0
                count +=1
                
            else: #case 1
                if case !=1 : 
                    sim_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 1
                count +=1
                husize = humansize(row['Access_Size'])
        else: 
            if row['type'] == 'open': #case 2
                if case !=2 : 
                    sim_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 2
                count +=1
            elif row['type'] == 'close': #case 3
                if case !=3 : 
                    sim_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 3
                count +=1
            else:
                if case !=4 : 
                    sim_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 4
                count +=1
    
    sim_add_row(type, case, access_record,count,filename,husize)
    access_record = access_record.iloc[1: , :]          
    print(access_record)
    return access_record

def agg_add_row(type,case,df,count,filename,size=''):
    if case == 0:
        row = [type,'Alternating 4KB or ~ 33KB',count,filename]
    elif case == 3:
        row = [type,'~ 9KB',count,filename]
    elif case == 4:
        row = [type,'16 Bytes',count,filename]
    elif case == 5:
        row = [type,'1200 Bytes',count,filename]
    elif case == 6:
        row = [type,size,count,filename]
    elif case == 7:
        row = [type,'-1',count,filename]
    elif case == 8:
        row = [type,'-1',count,filename]
    else:
        row = [type,'Meta',count,filename]

    df.loc[len(df.index)] = row

def agg_frame_report(df):
    access_record = pd.DataFrame(columns = ["Type", "Access","Count","Filename"])
    count = 0
    case = 0
    type = ''
    husize = ''
    filename = ''
    
    for index, row in df.iterrows():
        if row['Access_Region_Mem_Type'] == 'H5FD_MEM_DRAW':
            if (row['Access_Size'] == 4096) or (row['Access_Size'] < 34000 and row['Access_Size'] > 32000): # case 0
                if case !=0 : 
                    agg_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 0
                count +=1

            elif row['Access_Size'] > 9000 and row['Access_Size'] < 9500: # case 3
                if case !=3 : 
                    agg_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 3
                count +=1
            
            elif row['Access_Size'] == 16: # case 4
                if case !=4 : 
                    agg_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 4
                count +=1

            elif row['Access_Size'] == 1200: # case 5
                if case !=5 : 
                    agg_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 5
                count +=1
                
            else: #case 6
                if case !=6 : 
                    agg_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 6
                count +=1
                husize = humansize(row['Access_Size']) #humansize(row['Access_Size'])
        else: 
            if row['type'] == 'open': #case 7
                if case !=7 : 
                    agg_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 7
                count +=1
            elif row['type'] == 'close': #case 8
                if case !=8 : 
                    sim_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 8
                count +=1
            else:
                if case !=9 : 
                    agg_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 9
                count +=1
                
    
    agg_add_row(type, case, access_record,count,filename,husize)       
    access_record = access_record.iloc[1: , :]     
    print(access_record)
    return access_record

def agg_add_row2(type,case,df,count,filename,size=''):
    if case == 0:
        row = [type,'Alternate 4KB or ~ 33KB',count,filename]
    elif case == 3:
        row = [type,'Alternate ~ 9KB or 16 Bytes',count,filename]
    elif case == 5:
        row = [type,'1200 Bytes',count,filename]
    elif case == 6:
        row = [type,size,count,filename]
    elif case == 7:
        row = [type,'-1',count,filename]
    elif case == 8:
        row = [type,'-1',count,filename]
    else:
        row = [type,'Meta',count,filename]

    df.loc[len(df.index)] = row

def agg_frame_report2(df):
    access_record = pd.DataFrame(columns = ["Type", "Access","Count","Filename"])
    count = 0
    case = 0
    type = ''
    husize = ''
    filename = ''
    
    for index, row in df.iterrows():
        if row['Access_Region_Mem_Type'] == 'H5FD_MEM_DRAW':
            if (row['Access_Size'] == 4096) or (row['Access_Size'] < 34000 and row['Access_Size'] > 32000): # case 0
                if case !=0 : 
                    agg_add_row2(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 0
                count +=1

            elif (row['Access_Size'] == 16) or (row['Access_Size'] > 9000 and row['Access_Size'] < 9500): # case 3
                if case !=3 : 
                    agg_add_row2(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 3
                count +=1

            elif row['Access_Size'] == 1200: # case 5
                if case !=5 : 
                    agg_add_row2(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 5
                count +=1
                
            else: #case 6
                if case !=6 : 
                    agg_add_row2(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 6
                count +=1
                husize = humansize(row['Access_Size']) #humansize(row['Access_Size'])
        else: 
            if row['type'] == 'open': #case 7
                if case !=7 : 
                    agg_add_row2(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 7
                count +=1
            elif row['type'] == 'close': #case 8
                if case !=8 : 
                    agg_add_row2(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 8
                count +=1
            else:
                if case !=9 : 
                    agg_add_row2(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 9
                count +=1
                
    
    agg_add_row(type, case, access_record,count,filename,husize)       
    access_record = access_record.iloc[1: , :]     
    print(access_record)
    return access_record


def agg_res_check_row(type,curr_case, prev_case,df,count,filename,dfrow,size='',addon=0,addon2=0,pcsize=1740):
    if prev_case != curr_case:

        if prev_case == 0:
            if addon == 2000:
                row = [type,'Alternate 4KB or ~ 37KB',count,filename]
            elif addon == 42000:
                row = [type,'Alternate 4KB or ~ 77KB',count,filename]
            else:
                row = [type,'Alternate 4KB or ~ 34KB',count,filename]

        elif prev_case == 1:
            row = [type,'Alternate ~ 19KB or 16 Bytes',count,filename]
            if addon2 == 21000:
                row = [type,'Alternate ~ 40KB or 16 Bytes',count,filename]
            elif addon2 == 61000:
                row = [type,'Alternate ~ 80KB or 16 Bytes',count,filename]
            else:
                row = [type,'Alternate ~ 19KB or 16 Bytes',count,filename]

        elif prev_case == 2:
            row = [type, f"{pcsize} Bytes",count,filename]
        elif prev_case == 3:
            row = [type,size,count,filename]
        elif prev_case == 4:
            row = [type,'-1',count,filename]
        elif prev_case == 5:
            row = [type,'-1',count,filename]
        else:
            row = [type,'Meta',count,filename]

        df.loc[len(df.index)] = row
        count = 1
        prev_case = curr_case
    else:
        count +=1
    return count, dfrow['Filename'], dfrow['type'], prev_case

def agg_res_report2(df,residue):
    access_record = pd.DataFrame(columns = ["Type", "Access","Count","Filename"])
    count = 0
    case = 0
    type = ''
    husize = ''
    filename = ''

    addon = 0
    if residue == 210: addon = 2000
    if residue == 295: addon = 42000

    addon2 = 0
    if residue == 210: addon2 = 21000
    if residue == 295: addon2 = 61000

    pc_size = 1740
    if residue == 210: pc_size = 2520
    if residue == 295: pc_size = 3540
    
    for index, row in df.iterrows():
        if row['Access_Region_Mem_Type'] == 'H5FD_MEM_DRAW':
            if (row['Access_Size'] == 4096) or (row['Access_Size'] < (36000 + addon) and row['Access_Size'] > (33000 + addon)): # case 0
                count,filename,type,case= agg_res_check_row(type,0,case,access_record,count,filename,row,husize,addon,addon2,pc_size)

            elif (row['Access_Size'] == 16) or (row['Access_Size'] > (19000 + addon2) and row['Access_Size'] < (21000 + addon2)): # case 1
                count,filename,type,case= agg_res_check_row(type,1,case,access_record,count,filename,row,husize,addon,addon2,pc_size)

            elif row['Access_Size'] == pc_size: # case 2
                count,filename,type,case= agg_res_check_row(type,2,case,access_record,count,filename,row,husize,addon,addon2,pc_size)
                
            else: #case 3
                count,filename,type,case= agg_res_check_row(type,3,case,access_record,count,filename,row,husize,addon,addon2,pc_size)
                husize = humansize(row['Access_Size']) #humansize(row['Access_Size'])
        else: 
            if row['type'] == 'open': #case 4
                count,filename,type,case= agg_res_check_row(type,4,case,access_record,count,filename,row,husize,addon,addon2,pc_size)

            elif row['type'] == 'close': #case 5
                count,filename,type,case= agg_res_check_row(type,5,case,access_record,count,filename,row,husize,addon,addon2,pc_size)

            else:
                count,filename,type,case= agg_res_check_row(type,6,case,access_record,count,filename,row,husize,addon,addon2,pc_size)
                
    agg_res_check_row(type,100,case,access_record,count,filename,row,husize,addon,addon2,pc_size)
    access_record = access_record.iloc[1: , :]     
    print(access_record)
    return access_record


def sim_res_add_row(type,case,df,count,filename,size=''):
    if case == 0:
        row = [type,'~39KB',count,filename]
    elif case == 1:
        row = [type,'~ 41KB',count,filename]
    elif case == 2:
        row = [type,'~ 81KB',count,filename]
    elif case == 3:
        row = [type,size,count,filename]
    elif case == 4:
        row = [type,'-1',count,filename]
    elif case == 5:
        row = [type,'-1',count,filename]
    else:
        row = [type,'Meta',count,filename]

    df.loc[len(df.index)] = row

def sim_res_report(df):
    access_record = pd.DataFrame(columns = ["Type", "Access","Count","Filename"])
    count = 0
    case = 0
    type = ''
    husize = ''
    filename = ''
    
    for index, row in df.iterrows():
        if row['Access_Region_Mem_Type'] == 'H5FD_MEM_DRAW':
            if row['Access_Size'] < 40000 and row['Access_Size'] > 38000: # case 0
                if case !=0 : 
                    sim_res_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 0
                count +=1
            elif row['Access_Size'] < 42000 and row['Access_Size'] > 40000: # case 1
                if case !=1 : 
                    sim_res_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 1
                count +=1
            elif row['Access_Size'] < 82000 and row['Access_Size'] > 80000: # case 2
                if case !=2 : 
                    sim_res_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 2
                count +=1
                
            else: #case 3
                if case !=3 : 
                    sim_res_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 3
                count +=1
                husize = humansize(row['Access_Size'])
        else: 
            if row['type'] == 'open': #case 4
                if case !=4 : 
                    sim_res_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 4
                count +=1
            elif row['type'] == 'close': #case 5
                if case !=5 : 
                    sim_res_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 5
                count +=1
            else:
                if case !=6 : # case 6
                    sim_res_add_row(type, case, access_record,count,filename,husize)
                    filename = row['Filename']
                    type = row['type']
                    count = 0
                    case = 6
                count +=1
    
    sim_add_row(type, 5, access_record,count,filename,husize)
    access_record = access_record.iloc[1: , :]          
    print(access_record)
    return access_record

def plots(dir_path):
    res = []

    for path in os.listdir(dir_path):
        # check if current path is a file
        if os.path.isfile(os.path.join(dir_path, path)):
            if 'log.xlsx' in path:
                res.append(os.path.join(dir_path, path))
    print(res)
    print(len(res))

    for file in res:
        print(file)
        df = pd.read_excel(file)
        df.rename(columns = {'Unnamed: 0':'Order'}, inplace=True)
        plot_access(df, file)
        print()

def analyze(infile):
    str_list = infile.split(".")
    frame = int(str_list[1][1:])
    task = int(str_list[2][1:])
    residue = int(str_list[3][1:])

    df = pd.read_excel(infile)
    df.rename(columns = {'Unnamed: 0':'Order'}, inplace=True)

    if residue == 100:
        if 'sim' in infile:
            access_report = sim_frame_report(df)
            access_report.to_excel(f"{infile[:-9]}.report.xlsx")
        else:
            access_report = agg_frame_report(df)
            access_report.to_excel(f"{infile[:-9]}.report.xlsx")

            access_report2 = agg_frame_report2(df)
            access_report2.to_excel(f"{infile[:-9]}.report2.xlsx")
    else:
        if 'sim' in infile:
            access_report = sim_res_report(df)
            access_report.to_excel(f"{infile[:-9]}.report.xlsx")
        else:
            access_report = agg_res_report2(df,residue)
            access_report.to_excel(f"{infile[:-9]}.report2.xlsx")

def get_input():
    cmdargs = sys.argv
    if(len(cmdargs) < 2): exit("Please give input file")
    return cmdargs[1]

def process_all(dir_path):

    # list to store files
    res = []

    # Iterate directory
    for path in os.listdir(dir_path):
        # check if current path is a file
        if os.path.isfile(os.path.join(dir_path, path)):
            if 'log.xlsx' in path:
                res.append(os.path.join(dir_path, path))
    print(res)
    print(len(res))

    for infile in res:
        print(f"Analyzing {infile}...")
        analyze(infile)
        print(f"{infile} done.\n")


def show_report(dir_path):
    res = []

    for path in os.listdir(dir_path):
        # check if current path is a file
        if os.path.isfile(os.path.join(dir_path, path)):
            if 'report' in path:
                res.append(os.path.join(dir_path, path))
    print(res)
    print(len(res))

    for file in res:
        df = pd.read_excel(file)
        print(file)
        print(df.shape)
        print()

def main():
    # folder path
    dir_path = 'save_job_log/4627_job_log'
    # 'save_job_log/1352_job_log'   'save_job_log/4627_job_log' 'save_job_log/2375_job_log'
    # 'save_job_log/2024_job_log' 'save_job_log/5590_job_log' # not good

    cmdargs = sys.argv
    if(len(cmdargs) < 2): exit("Please input file (with path) or 'all' or 'show'")
    inarg = cmdargs[1]

    if inarg == 'all':
        process_all(dir_path)
    elif inarg == 'show':
        show_report(dir_path)
    elif inarg == 'plot':
        plots(dir_path)
    else:
        analyze(inarg)

    # plots(infile)
    
    # plot_hist(df['Access_Size'])

if __name__ == "__main__":
    main()