import pandas as pd
import os
import glob
import networkx as nx
import caterpillar


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


    def get_critical_path(self, G=None, weight='weight'):
        G = self.g if G is None else G
        cpath = nx.dag_longest_path(G, weight=weight)
        self.cpath = cpath
        return self.cpath


    def get_neighbor_leaves(self, G=None, cpath=None):
        cpath = self.cpath if cpath is None else cpath
        G = self.g if G is None else G
        nb_leaves = {}
        for node in cpath:
            neighbors = set([x[0] for x in G.in_edges(node)] + [x[1] for x in G.out_edges(node)])
            nb_leaves[node] = neighbors - set(cpath)
        self.neighbor_leaves = nb_leaves
        return self.neighbor_leaves


    def caterpillar_tree(self, G=None, cpath=None, weight='weight'):
        cpath = self.get_critical_path(weight=weight) if cpath is None else cpath
        G = self.g if G is None else G

        nb_leaves = self.get_neighbor_leaves(G, cpath)
        ctree_graph = caterpillar_tree_in_graph(G, nb_leaves, weight=weight)
        self.ctree_graph = ctree_graph
        return self.ctree_graph


    def get_graph(self):
        """Returns NetworkX DiGraph built from loaded stats
        """

        g = self.g
        df_all = self.get_df_all()
        key_idx = self.stat_key_idx        
        for tname, fnames in df_all.items():
            g.add_node(tname, ntype='task')    
            for fname, _df in fnames.items():
                if 'r' in _df:
                    size_r = (_df['r'][key_idx[1]] * _df['r'][key_idx[2]])
                else:
                    size_r = pd.DataFrame()
                size_r = pd.Series([0]) if size_r.empty else size_r
                if 'w' in _df:
                    size_w = (_df['w'][key_idx[1]] * _df['w'][key_idx[2]])
                else:
                    size_w = pd.DataFrame()
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


def _new_graph_with_top_x(G, df, metric_name, edges_to_collect="both", count=10, ascending=False):

    new_G = nx.DiGraph()
    t = df.sort_values(by=[metric_name], ascending=ascending)
    cnt = 0
    for idx, row in t.iterrows():
        if(len(row[0]) > 1 and isinstance(row[0][0], tuple)):
            iedge, oedge = row[0][0], row[0][1]
            ival = G.edges[iedge]['value']
            new_G.add_edges_from([iedge], value=1)
            oval = G.edges[oedge]['value']
            new_G.add_edges_from([oedge], value=1)
        elif (len(row[0]) > 1 and isinstance(row[0], tuple)):
            edge = row[0]
            val = G.edges[edge]['value']
            new_G.add_edges_from([edge], value=1)
        else:
            node_name = row[0]
            if edges_to_collect == "in" or edges_to_collect == "both":
                for edge in G.in_edges(node_name):
                    val = G.edges[edge]['value']
                    new_G.add_edges_from([edge], value=1)
            if edges_to_collect == "out" or edges_to_collect == "both":
                for edge in G.out_edges(node_name):
                    val = G.edges[edge]['value']
                    new_G.add_edges_from([edge], value=1)
        cnt += 1
        if cnt >= count:
            break
    return new_G


def _edge_attr_in_unit(G, unit='byte', metric='value'):

    col1 = []
    col2 = []
    is_round = True
    if unit.lower() == "gb":
        unit_size = 10e8
    elif unit.lower() == "mb":
        unit_size = 10e5
    elif unit.lower() == "kb":
        unit_size = 10e2
    else:
        unit = "byte"
        unit_size = 10e-1
        
    for u, v, attr in G.edges(data=True):
        col1.append((u, v))
        val = attr[metric]
        col2.append(round(val / unit_size, 2)  if is_round else val)
    
    return pd.DataFrame({f'node (edge)':col1, 
                         f'{metric} ({unit})': col2})


def producer_consumer_ranking_table(G, unit='GB', metric='value'):
    df = _edge_attr_in_unit(G, unit, metric)
    val = f'value ({unit})'
    return df.sort_values(by=val, ascending=False)


def data_branches_and_task_joins(orig_G, unit="GB", sort_order='descending'):

    df = _edge_attr_in_unit(orig_G, unit)
    val = f'value ({unit})'
    is_ascend = False if sort_order == 'descending' else True
    df.sort_values(by=val, ascending=is_ascend)
    num_rows_to_select = len(df)
    new_G = _new_graph_with_top_x(orig_G, df, val, count=num_rows_to_select, ascending=is_ascend)
    return new_G


def get_critical_path(G, weight='weight'):
    return nx.dag_longest_path(G, weight=weight)


def get_critical_path_edges(G, weight='weight'):
    """Take a networkx graph G with a weight to provide
    a list of edge tuples identified as a critical path
    dag_longest_path provides node list in a DiGraph
    """
    path = get_critical_path(G, weight)
    path_edges = list(zip(path,path[1:]))
    return path_edges


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


def caterpillar_tree_in_graph(G, nb_leaves, weight='weight'):

    caterpillar_graph = nx.DiGraph()
    prev = None
    for k, v in nb_leaves.items():
        v = list(v)
        for v_i in range(len(v)):
            try:
                caterpillar_graph.add_edge(k, v[v_i], value=G.get_edge_data(k, v[v_i])[weight])
            except:
                caterpillar_graph.add_edge(v[v_i], k, value=G.get_edge_data(v[v_i], k)[weight])
        if prev is not None:
            caterpillar_graph.add_edge(prev, k, value=G.get_edge_data(prev, k)[weight])
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
    cf = nx.DiGraph()
    while(G.nodes or G.edges):
        # find a critical path
        cpath = get_critical_path(G)
        # extract the CT along the critical path
        CT_c = caterpillar_tree(G, cpath)
        # Remove the vertices and edges only along the critical path
        removed_g = remove_cpath_from_graph(G, cpath)
        # (find dependencies across CT_s) for each of the vertices,
        # v_c on the critical path of the current caterpillar tree, CT_c
        for ct_i in CT_s:
            for node in CT_c.nodes:
                # if there is an edge between v_p and v_c,
                # add a dependency edge between CT_c  and ct_i
                o_edges = ct_i.out_edges(node)
                i_edges = ct_i.in_edges(node)
                tmp = list(o_edges) + list(i_edges)
                dependent_edges += tmp
                if len(tmp) > 0:
                    if(len(o_edges) > 0):
                        cf.add_edge(CT_c, ct_i)
                    if(len(i_edges) > 0):
                        cf.add_edge(ct_i, CT_c)

        dependent_edges = list(set(dependent_edges))
        CT_s.append(CT_c)
        G = removed_g
    return (cf, dependent_edges)


def get_roots(G):
    roots = []
    for ct_node in G.nodes:
        pred_iter = G.predecessors(ct_node)
        pred_l = [x for x in pred_iter]
        if len(pred_l) == 0:
            roots.append(ct_node)
    return roots


def get_dependent_ct_count(G, ctree_list):
    res = {}
    for ctree in ctree_list:
        succ_iter = G.successors(ctree)
        succ_l = [x for x in succ_iter]
        res[ctree] = len(succ_l)
    return res


def get_first_stalls(G, d_edges):
    d_edges_flattened = []
    for edge in d_edges:
        d_edges_flattened += edge
    d_edges_flattened = list(set(d_edges_flattened))
    found_node = None
    for node in d_edges_flattened:
        tmp = G.edges(node)
        if len(tmp) > 0:
            found_node = node
            break
    first_stalls = []
    if found_node is not None:
        tmp = [found_node]
        while (tmp):
            curr = tmp.pop()
            pred_iter = G.predecessors(curr)
            pred_l = [x for x in pred_iter]
            if len(pred_l) > 0:
                tmp = pred_l
                first_stalls += pred_l
    return first_stalls


def get_edge_attributes(G, metric='value'):
    return [x[2][metric] for x in G.edges(data=True)]


def find_vertex_priority(cforest, G_list, dependent_edges, metric='value', op='sum'):
    cts_cnt = get_dependent_ct_count(cforest, G_list)
    res = {}
    for G in G_list:
        first_stalls = get_first_stalls(G, dependent_edges)
        attrs = get_edge_attributes(G, metric)
        func = getattr(np, op)
        mvalue = func(attrs)
        res[G] = (cts_cnt[G], len(first_stalls), mvalue)
    return res
