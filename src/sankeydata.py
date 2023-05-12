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
    
    def add_node(self, node_dict):
        # label, color, customdata, x, y
        x = pd.DataFrame([node_dict])
        self.nodes = pd.concat([self.nodes, x], ignore_index=True)
        new_id = len(self.nodes)
        
    def get_link(self, name):
        return self.link[name]
    
    def set_links(self):
        pass
    
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
    
    def plot(self, nodes=None, links=None):
        n = nodes or self.nodes
        n = n[['label','color']].to_dict('list')
        l = links or self.links
        l = l.to_dict('list')
        fig = go.Figure(data=[go.Sankey(
            node = n,
            link = l, orientation='h')])
        fig.show()    

    def reset(self):
        del(self.df_stat)
        del(self.nodes)
        del(self.links)    
