import pandas as pd
import os
import glob

class DataLife(object):
    """Data Lifecycles for Optimizing Workflow Task & Data Coordination
    
    Args:
        dl_name (str): name for data lifecycles
    """
    def __init__(self, dl_name=''):
        self.wf_tasknames = []
        self.stat_path = ''
        self.dl_name = dl_name
        self.stat_in_df_all = {}
        
        
    def set_wf_tasknames(self, names):
        '''Sets workflow task names in a list
        
            Parameters:
                names (list): list of names
        '''
        if isinstance(names, list): 
            self.wf_tasknames = names
        else:
            self.wf_tasknames.append(names)
            
            
    def set_dl_name(self, dl_name):
        """Sets datalife name
        
        Args:
            dl_name (str): name of datalife
        """
        self.dl_name = dl_name

        
    def read_stats(self, spath):
        """Reads stat files
        
        Args:
            spath (str): path name to read stat files
            
        """
        if spath is None or len(spath) == 0:
            return
        
        self.stat_path = spath
        self._read_stat_with_wf_tasks()
        

    def _read_stat_with_wf_tasks(self):
        """Reads stat files through workflow tasks and store in DataFrame
        """
        
        task_names = self.wf_tasknames
        stat_path = self.stat_path
        dl_name = self.dl_name
        df_dict_all = {}

        for task_name in task_names:
            df_dict = {}
            for ftype in ["r", "w"]:
                fnames = glob.glob(os.path.join(stat_path, task_name, f'*{ftype}_stat'))
                for fname in fnames:
                    bname = os.path.basename(fname)[:-7] # -7: _x_stat
                    df = self._read_stat(fname)
                    if bname in df_dict:
                        if ftype in df_dict[bname]:
                            if concat is True:
                                df_orig = df_dict[bname][ftype]
                                df = pd.concat([df_orig, df])        
                            else:
                                print('--overwrite-- [%s, %s]'.format(bname, ftype))
                                df_dict[bname][ftype] = df
                        else:
                            df_dict[bname][ftype] = df
                    else:
                        df_dict[bname] = {ftype:df}

            df_dict_all[task_name] = df_dict
        self.stat_in_df_all = df_dict_all

        
    def _read_stat(self, stat_fname, column_names=['block_idx', 'frequency', 'access_size'], skip_header_rows=1):
        """Returns a Pandas DataFrame by reading stat files

            Parameters:
                stat_fname (str): stat filename
                column_names (list): column names
                skip_header_rows (int): number of header lines to skip, default is 1

            Returns:
                df (DataFrame): Pandas DataFrame
        """
        df = pd.read_csv(stat_fname, sep=' ', names=column_names, skiprows=skip_header_rows)
        return df

    
    def get_df_all(self):
        """Returns loaded stats in Pandas Dataframe
        """
        return self.stat_in_df_all
    
    
    def get_stat_summary(self):
        """Returns summary of data lifecycles along with workflow tasks
        """
        df_all = self.get_df_all()
        task_cnt = len(df_all.keys())
        stat_fcnt = 0
        io_size_r = 0
        io_size_w = 0
        for k1, v1 in df_all.items():
            stat_fcnt += len(v1.keys())
            for k2, v2 in v1.items():
                io_size_r += v2['r'].access_size.sum() if 'r' in v2 else 0
                io_size_w += v2['w'].access_size.sum() if 'w' in v2 else 0
        data = {'number of workflow tasks': task_cnt,
               'number of stat files': stat_fcnt,
               'io size in read (byte)': io_size_r,
               'io size in write (byte)': io_size_w}
        column_names = ['value']
        summary = pd.DataFrame.from_dict(data, orient='index',
                       columns=column_names)
        return summary
