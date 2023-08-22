import pandas as pd
import plotly.graph_objects as go

class SankeyData:
    
    # From Tazer
    df_stat = list()
    # Sankey in Plotly, node - vertex, link - edge
    nodes = pd.DataFrame(columns=['label', 'color'])
    links = pd.DataFrame(columns=['source', 'target', 'value'])
    
    color_map = {"task": "red",
                 "file": "blue",
                 "none": "grey"
                }
    block_size = 1024
        
    def __init__(self):
        pass

    def import_graph(self, G, color_map=None):

        self.g = G
        self.color_map = color_map if color_map is not None else self.color_map
        node_dicts, node_idx = self._get_nodes_from_graph()
        self.g_node_idx = node_idx
        self.g_node_dicts = node_dicts
        link_dicts = self._get_links_from_graph()
        self.set_nodes(node_dicts)
        self.set_links(link_dicts)

    def _get_nodes_from_graph(self, G=None, color_map=None):

        G = G if G is not None else self.g
        color_map = color_map if color_map is not None else self.color_map

        node_idx = {}
        node_dict_for_sankey = {'label': [], 'color':[], 'x':[], 'y':[]}

        for idx, (node_name, attr) in enumerate(G.nodes(data=True)):
            node_type = 'task' if 'ID' in node_name else 'file'
            if node_name in node_idx:
                pass
            node_idx[node_name] = {'idx':idx, 'type':node_type}

            if "producer" in color_map and "consumer" in color_map:
                if(node_type == "task" and len(G.in_edges(node_name)) < 1):
                    node_type = "producer"
                elif(node_type == "task" and len(G.out_edges(node_name)) < 1):
                    node_type = "consumer"
            node_dict_for_sankey['label'].append(node_name)
            node_dict_for_sankey['color'].append(color_map[node_type])
        return node_dict_for_sankey, node_idx

    def _get_links_from_graph(self, G=None, node_idx=None, metric='value'):
        
        G = G if G is not None else self.g
        node_idx = node_idx if node_idx is not None else self.g_node_idx

        link_dict_for_sankey = {'source':[], 'target':[], 'value':[]}
        for u, v, attr in G.edges(data=True):
    #         print(u, v, attr)
            u_idx = node_idx[u]['idx']
            v_idx = node_idx[v]['idx']
            link_dict_for_sankey['source'].append(u_idx)
            link_dict_for_sankey['target'].append(v_idx)
            link_dict_for_sankey['value'].append(attr[metric])
        return link_dict_for_sankey
            
    def load_stat(self, df_stat, params):
            
            self.df_stat.append({ "df": df_stat,
                                  "filename": params['filename'],
                                  "type": params['type'],
                                  "size": params['size'],
                                 "task_name": params['task_name']
                                })

            # add a node entry (task)
            self.set_taskname(params['task_name'])

            # add a node entry (input/output file)
            # one filename entry is allowed
            if len(self.nodes[self.nodes.label == params['filename']]) == 0:
                self.add_node({'label': params['filename'],
                           'color': self.color_map['file']})

    def load_stat_all(self, df_all):
        for task_name, df_ in df_all.items():
            for bname, df__ in df_.items():
                for ftype in ['r', 'w']:
                    if ftype in df__:
                        self.load_stat(df__[ftype], {'filename':bname,
                                                      'type': ftype,
                                                      'size': 0,
                                                      'task_name': task_name})
        
    def set_taskname(self, name):
        if len(self.nodes[self.nodes.label == name]) == 0:
            self.add_node({'label': name,
                           'color': self.color_map['task']})
                    
    def get_node(self, idx):
        return self.nodes[idx]

    def set_nodes(self, data):
        if isinstance(data, dict):
            nodes = pd.DataFrame(data, columns=['label', 'color'])
        else:
            nodes = data
        self.nodes = nodes
    
    def add_node(self, node_dict):
        # label, color, customdata, x, y
        x = pd.DataFrame([node_dict])
        self.nodes = pd.concat([self.nodes, x], ignore_index=True)
        new_id = len(self.nodes)
        
    def get_link(self, name):
        return self.link[name]
    
    def set_links(self, data):
        if isinstance(data, dict):
            links = pd.DataFrame(data, columns=['source', 'target', 'value'])
        else:
            links = data
        self.links = links
    
    def build_links(self, no_rw=False):

        links = []
        #key1 = 'block_idx'
        key2 = 'frequency'
        key3 = 'access_size'
        for v in self.df_stat:
            # input (r)
            #cnt = v['df'][key].nunique()
            tmp = v['df'][key2] * v['df'][key3]
            cnt = tmp.sum()
            io_type = v['type']
            fname = v['filename']
            tname = v['task_name']
            fidx = int(self.nodes[self.nodes.label == fname].index.values)
            tidx = int(self.nodes[self.nodes.label == tname].index.values)
            if io_type == "r":
                t2f = {'source': fidx,
                       'target': tidx,
                       'value': cnt}
                if no_rw:
                    t2n = {'source': fidx + 1,
                           'target': tidx,
                           'value': (v['size'] / self.block_size) - cnt}
            else:
                t2f = {'source': tidx,
                       'target': fidx,
                       'value': cnt}
                if no_rw:
                    t2n = {'source': tidx,
                           'target': fidx + 2,
                           'value': (v['size'] / self.block_size) - cnt}

            links.append(t2f) 
            links.append(t2n) if no_rw else None

        links = pd.DataFrame(links)
        self.links = links

    def oneway_direct(self, links=None):
        if links is None:
            links = self.links
        del_list = []
        for index, row in links.iterrows():
            source = row['source']
            target = row['target']
            value = row['value']
            is_found = links.loc[(links['source'] == target) & (links['target'] == source) & (links['value'] > value)]
            if len(is_found) > 0:
                del_list.append(index)
        return del_list


    def highlight_edge_colors(self, G=None, cpath='purple', branch_join='lightgreen'):
        G = self.g if G is None else G
        highlight_color1 = 'rgb(204,204,204)' # gray default color
        highlight_color2 = 'rgb(204,204,204)'
        if cpath is not None:
            highlight_color1 = cpath
        if branch_join is not None:
            highlight_color2 = branch_join

        from datalife import get_critical_path_edges
        self.links['color'] = _highlight_edge_color(G, get_critical_path_edges(G), highlight_color1, highlight_color2)


    def plot(self, nodes=None, links=None, options=None):
        n = self.nodes if nodes is None else nodes
        n = n[['label','color']].to_dict('list')
        l = links or self.links
        l = l.to_dict('list')
        orientation = 'h'
        outfname = None
        if options:
            if 'no_label' in options:
                if options['no_label'] is True:
                    n['label'] = ['' for x in n['label']]
            if 'orientation' in options:
                if options['orientation'] == 'v':
                    orientation = 'v'
            if 'save_image' in options:
                outfname = options['save_image']

        fig = go.Figure(data=[go.Sankey(
            node = n,
            link = l, orientation=orientation)])
        fig.write_image(outfname) if outfname is not None else ''
        fig.show()

    def reset(self):
        del(self.df_stat)
        del(self.nodes)
        del(self.links)    


def _highlight_edge_color(G, edge_tuples, highlight_color='purple', highlight_color_ext='lightgreen'):
    """Take a networkx graph G with a list of edge tuples
    to change color of edges towards a sankey diagram"""
    color_list = []
    for u, v, attr in G.edges(data=True):
        is_matched = False
        is_matched_ext = False
        for u1, v1 in edge_tuples:
            if u == u1 and v == v1:
                is_matched = True
                break
            if u == u1 or v == v1:
                is_matched_ext = True
                break
        if is_matched_ext:
            color_list.append(highlight_color_ext)
        elif is_matched:
            color_list.append(highlight_color)
        else:
            color_list.append('rgb(204,204,204)') # gray
    return color_list


