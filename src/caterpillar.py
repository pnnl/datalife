import networkx as nx

default_resource_set = {'cpu': 32,
                          'data_rate_write':300, 'data_rate_read': 3000,
                       'data_volume': 1024*1024 }


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


def get_roots(G):
    roots = []
    for ct_node in G.nodes:
        pred_iter = G.predecessors(ct_node)
        pred_l = [x for x in pred_iter]
        if len(pred_l) == 0:
            roots.append(ct_node)
    return roots


def task_nodes(G):
    res = []
    for node, attr in G.nodes(data=True):
        if 'ntype' in attr and attr['ntype'] == 'task':
            res.append(node)
    return res


def data_nodes(G):
    res = []
    for node, attr in G.nodes(data=True):
        if not 'ntype' in attr or attr['ntype'] != 'task':
            res.append(node)
    return res


def max_cpu_req(G):
    tnode_names = task_nodes(G)
    cpu_cnt = 0
    for tnode in tnode_names:
        # TODO 
        # - 2depth search for concurrent tasks (adding cpu counts, not max value among them)
        #for node in nx.neighbors(G, tnode):
        cpu_cnt = max(cpu_cnt, G.nodes[tnode]['cpu'])
    return cpu_cnt

def total_exec_time(G):
    tnode_names = task_nodes(G)
    time_length = 0
    for tnode in tnode_names:
        time_length += G.nodes[tnode]['estimated_in_second']
    return time_length


def resource_required_for_caterpillar(G):#, resource_grp = default_resource_set):
    '''return total sum value of concurrent vertex resource attributes'''
    max_cpus = max_cpu_req(G)
    time_length = total_exec_time(G)
    #resource_max = resource_grp.copy()
    # TODO
    # - source to sink nodes (verifying edges return them in order)
    # TODO
    # - abstract resource entities
    total_volume = 0
    max_rate_r = 0
    max_rate_w = 0
    for u, v, attr in G.edges(data=True):
        total_volume += attr['data_volume']
        max_rate_r = max(max_rate_r, (attr['data_rate_read'] or 0))
        max_rate_w = max(max_rate_w, (attr['data_rate_write'] or 0))        
        '''
        for n in nx.neighbors(G, ct_node):
            print(n)
            resource_tmp = {k: 0 for k, v in resource_grp.items()}
            for k, v in resource_grp.items():
                if k in G.nodes[n]:
                    resource_tmp[k] += G.nodes[n][k]
                else:
                    print(n)
                    resource_tmp[k] += resource_grp[k] # default val
        for k, v in resource_grp.items():
            resource_max[k] = max(resource_max[k], resource_tmp[k])
        '''
    return {'cpu': max_cpus,
            'max_rate_r': max_rate_r,
            'max_rate_w': max_rate_w,
            'volume': total_volume,
            'estimated_in_second': time_length}#resource_max     


# find_caterpillar_forest in code
def caterpillars(G):
    
    annotation = {}
    CT_s = []
    dependent_edges = []
    dependent_i_edges = []
    dependent_o_edges = []
    cf = nx.DiGraph()
    x = y = 0
    DEBUG = False
    ct_level = 0
    while(G.nodes or G.edges):
        print("###") if DEBUG is True else None
        # find a critical path
        cpath = get_critical_path(G)
        # extract the CT along the critical path
        CT_c = caterpillar_tree(G, cpath)
        # Remove the vertices and edges only along the critical path
        removed_g = remove_cpath_from_graph(G, cpath)
        ct_segment_first_found = None
        queue_to_update = []
        # (find dependencies across CT_s) for each of the vertices,
        # v_c on the critical path of the current caterpillar tree, CT_c
        is_dependent_o_edges = is_dependent_i_edges = 0

        for ct_i in CT_s:
            is_dependent_o_edges = is_dependent_i_edges = 0
            node_cache = []
            print('===') if DEBUG is True else None
            nodes_in_CT_c = list(CT_c.nodes)
            node = nodes_in_CT_c.pop(0)
            while node:
                print("start ", node) if DEBUG is True else None
                # if there is an edge between v_p and v_c, 
                # add a dependency edge between CT_c  and ct_i
                o_edges = ct_i.out_edges(node)
                i_edges = ct_i.in_edges(node)
                dependent_i_edges += list(i_edges)
                dependent_o_edges += list(o_edges)
                u_of_edge = v_of_edge = None
                #ode_cache.append(node)
                if(len(o_edges) > 0):
                    u_of_edge = CT_c
                    v_of_edge = ct_i
                    # prevent cycle
                    #if is_dependent_ct_found > 0 and len(dependent_i_edges) > 0:
                    #    u_of_edge = CT_c.copy()
                    #    x += 1
                    is_dependent_o_edges += 1
                if(len(i_edges) > 0):
                    u_of_edge = ct_i
                    v_of_edge = CT_c
                    #if is_dependent_ct_found > 0 and len(dependent_o_edges) > 0:
                    #    v_of_edge = CT_c.copy()
                    #    x += 1                        
                    is_dependent_i_edges += 1
                print("is 1:", is_dependent_i_edges, is_dependent_o_edges) if DEBUG is True else None
                if is_dependent_i_edges > 0 and is_dependent_o_edges > 0:
                    #node_cache.append(node)
                    ct_segment_remaining = remove_cpath_from_graph(CT_c, node_cache)
                    CT_s.append(nx.subgraph(CT_c, node_cache))
                    is_dependent_o_edges = is_dependent_i_edges = 0
                    node_cache = []
                    CT_c = ct_segment_remaining
                    nodes_in_CT_c = list(ct_segment_remaining.nodes)
                    node = nodes_in_CT_c.pop(0)
                    print(ct_segment_remaining.nodes) if DEBUG is True else None
                    print("****") if DEBUG is True else None
                    continue
                    #break
                else:#if is_dependent_i_edges == 0 and is_dependent_o_edges == 0:
                    node_cache.append(node)
                    print("node, cache:", node,node_cache) if DEBUG is True else None
                print("is:", is_dependent_i_edges, is_dependent_o_edges) if DEBUG is True else None
                    
                if u_of_edge and v_of_edge:
                    edge_type = 'in' if len(i_edges) > 0 else 'out'
                    queue_to_update.append([edge_type, u_of_edge, v_of_edge, x, y])  
                print("nodes", nodes_in_CT_c) if DEBUG is True else None
                node = nodes_in_CT_c.pop(0) if len(nodes_in_CT_c) > 0 else []
                # end while loop for node search #

            # prevent cyclic
            print(queue_to_update) if DEBUG is True else ''
            cnt = 0
            for edge_type, u, v, x, y in queue_to_update:
                cf.add_edge(u, v)
                if 'ct_level' in u:
                    ct_level = u['ct_level']
                cf.add_node(u, pos=(x, y), ct_level=ct_level, ct_depth=cnt)
                x += 1
                y += 1
                ct_level_ = ct_level
                if 'ct_level' in v:
                    ct_level_ = u['ct_level']
                else:
                    ct_level_ = ct_level + 1
                cf.add_node(v, pos=(x, y), ct_level=ct_level_, ct_depth=cnt)
                x += 1
                y += 1
                cnt += 1
        
        ct_level += 1
        annotation[CT_c] = {'dependent_ct_count': is_dependent_o_edges + is_dependent_i_edges}
        dependent_edges = list(set(dependent_i_edges + dependent_o_edges))
        CT_s.append(CT_c)
        G = removed_g
    return cf#, dependent_edges, annotation)
