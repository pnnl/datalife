import pandas as pd
import os
import glob
import networkx as nx


class DataLife(object):
    """Data Lifecycles for Optimizing Workflow Task & Data Coordination
    
    Args:
        dl_name (str): name for data lifecycles
    """
    def __init__(self, dl_name=''):
        self.g = nx.DiGraph()
        self.wf_tasknames = []
        self.stat_path = ''
        self.dl_name = dl_name
        self.stat_in_df_all = {}
        self.stat_key_idx = ['block_idx', 'frequency', 'access_size']
        
        
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


    def get_graph(self):
        """Returns NetworkX DiGraph built from loaded stats
        """

        g = self.g
        df_all = self.get_df_all()
        key_idx = self.stat_key_idx        
        for tname, fnames in df_all.items():
            g.add_node(tname, ntype='task')    
            for fname, _df in fnames.items():
                size_r = (_df['r'][key_idx[1]] * _df['r'][key_idx[2]])
                size_r = pd.Series([0]) if size_r.empty else size_r
                size_w = (_df['w'][key_idx[1]] * _df['w'][key_idx[2]])
                size_w = pd.Series([0]) if size_w.empty else size_w
                size_r = size_r.sum()
                size_w = size_w.sum()
                if size_w > size_r:
                    g.add_edge(tname, fname, value=size_w)
                else:
                    g.add_edge(fname, tname, value=size_r) 

        return self.g       

    
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


def get_critical_path(G, weight='weight'):
    return nx.dag_longest_path(G, weight=weight)


def get_neighbor_leaves(G, cpath):
    nb_leaves = {}
    for node in cpath:
        #neighbors = set(G.neighbors(node)) #| set(tmp)
        neighbors = set([x[0] for x in G.in_edges(node)] + [x[1] for x in G.out_edges(node)])
        nb_leaves[node] = neighbors - set(cpath)
    return nb_leaves


def max_leaves(nb_leaves):
    return max(len(v) for v in nb_leaves.values())


def node_space(nb_leaves):
    return 1 + int((nb_leaves - 1) / 2)


def space_needed(nb_leaves):
    return 1 + sum(node_space(len(v)) for v in nb_leaves.values())


def caterpillar_tree_in_graph(G, nb_leaves):

    caterpillar_graph = nx.DiGraph()
    prev = None
    for k, v in nb_leaves.items():
        v = list(v)
        for v_i in range(len(v)):
            try:
                caterpillar_graph.add_edge(k, v[v_i], value=G.get_edge_data(k, v[v_i])['weight'])
            except:
                caterpillar_graph.add_edge(v[v_i], k, value=G.get_edge_data(v[v_i], k)['weight'])
        if prev is not None:
            caterpillar_graph.add_edge(prev, k, value=G.get_edge_data(prev, k)['weight'])
        if k in G.nodes and 'ntype' in G.nodes[k] and G.nodes[k]['ntype'] == 'task':
            caterpillar_graph.add_node(k, ntype='task')
        for vv in v:
            if vv in G.nodes and 'ntype' in G.nodes[vv] and G.nodes[vv]['ntype'] == 'task':
                caterpillar_graph.add_node(vv, ntype='task')
        prev = k
    return caterpillar_graph


def caterpillar_tree(G, cpath=None):

    if cpath is None:
        cpath = get_critical_path(G)

    nb_leaves = get_neighbor_leaves(G, cpath)
    ct_graph = caterpillar_tree_in_graph(G, nb_leaves)
    return ct_graph


def remove_cpath_from_graph(G, cpath):
    Gtmp = G.copy()
    Gtmp.remove_nodes_from(cpath)
    return Gtmp


def find_caterpillar_forest(G):
    CT_s = []
    dependent_edges = []
    while(G.nodes or G.edges):
        # find a critical path
        cpath = get_critical_path(G)
        # extract the CT along the critical path
        CT_c = caterpillar_tree(G, cpath)
        # Remove the vertices and edges only along the critical path
        removed_g = remove_cpath_from_graph(G, cpath)
        # (find dependencies across CT_s) for each of the vertices,
        # v_c on the critical path of the current caterpillar tree, CT_c
        for node in CT_c.nodes:
            for ct_i in CT_s:
                # if there is an edge between v_p and v_c, 
                # add a dependency edge between CT_c  and ct_i
                o_edges = ct_i.out_edges(node)
                i_edges = ct_i.in_edges(node)
                dependent_edges += list(o_edges) + list(i_edges)
        dependent_edges = list(set(dependent_edges))
        CT_s.append(CT_c)
        G = removed_g    
    return (CT_s, dependent_edges)
