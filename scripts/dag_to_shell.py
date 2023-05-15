import yaml
import sys
from pprint import pprint
import argparse

def convert2Dict(dlist, id_name):
    return {x[id_name]: x for x in dlist}


def import_yaml(fname):
    with open(fname) as f:
        return yaml.safe_load(f)


class Conversion(object):

    data = None
    job_dependency = None
    tasks = None
    cmds = None
    files = None


    def user_input(self):
        parser = argparse.ArgumentParser('Pegasus DAG Conversion Script')
        parser.add_argument('-w', '--workflow_yml')
        parser.add_argument('-r', '--replica_yml')
        parser.add_argument('-t', '--transformation_yml')
        parser.add_argument('-p', '--radical_pst_yml', help='output yaml filename')
        parser.add_argument('-o', '--output', help='shell script output')
        args = parser.parse_args()
        self.parser = parser
        self.args = args
        return args


    def abort(self, msg=''):
        print("ERROR: %s\n" % msg)
        self.parser.print_help()
        sys.exit(-1)

    def import_dax(self):

        data = None
        jdep = None
        tasks = None
        files = None
        cmds = None

        if self.args.workflow_yml:
            data = import_yaml(self.args.workflow_yml)
            jdep = data['jobDependencies']

            if 'transformationCatalog' in data:
                tasks = convert2Dict(data['transformationCatalog']['transformations'], 'name')
            if 'jobs' in data:
                cmds = convert2Dict(data['jobs'], 'id')
            if 'replicaCatalog' in data:
                files = convert2Dict(data['replicaCatalog']['replicas'], 'lfn')
        if self.args.replica_yml:
            repl = import_yaml(self.args.replica_yml)
            files = convert2Dict(repl['replicas'], 'lfn')
        if self.args.transformation_yml:
            trans = import_yaml(self.args.transformation_yml)
            tasks = convert2Dict(trans['transformations'], 'name')

        self.data = data
        self.job_dependency = jdep
        self.tasks = tasks
        self.cmds = cmds
        self.files = files

        if jdep is None:
            self.abort('Job Dependency is None')


    def get_jdep(self):
        dict_dep = {}

        for i in self.job_dependency:
            dict_dep[i['id']] = i['children']

        ordered = {}
        for pjob_id, child_ids in dict_dep.items():
            if pjob_id not in ordered:
               ordered[pjob_id] = 1
            for cjob_id in child_ids:
                if cjob_id not in ordered:
                   ordered[cjob_id] = ordered[pjob_id] + 1
                else:
                   ordered[cjob_id] = max(ordered[pjob_id] + 1, ordered[cjob_id])
        self.ordered = ordered
        self.ordered_by_val = {k: v for k, v in sorted(ordered.items(), key=lambda item: item[1])}


    def get_cmd(self, cid):
        cname = self.cmds[cid]['name']
        #     {'name': 'mConvert', 'sites': [{'name': 'local', 'pfn':
        #     '/files0/oddite/leeh736/Montage/bin/mConvert', 'type': 'stageable'}],
        #     'profiles': {'condor': {'request_memory': '1 GB'}, 'env': {'PATH':
        #     '/usr/bin:/bin:.'}}}
        t_path = self.tasks[cname]['sites'][0]['pfn']
        return t_path


    def get_arguments(self, cid):
        return self.cmds[cid]['arguments']


    def get_data(self, cid):

        inputs = []
        outputs = []
        pre_exec = []

        flist = self.cmds[cid]['uses']
        # uses: 
        # - lfn: pposs2ukstu_blue_001_002_area.fits
        #   type: output
        #   stageOut: false
        #   registerReplica: true
        # - lfn: poss2ukstu_blue_001_002.fits
        #   type: input
        for v in flist:
            fname = v['lfn']
            ftype = v['type']
            # - lfn: region-oversized.hdr
            #   pfns:
            #   - site: local
            #     pfn: file:///files0/oddite/leeh736/montage-workflow-v3/data/region-oversized.hdr
            if fname in self.files:
                fobj = self.files[fname]
                flocation = fobj['pfns'][0]['site']
                fpath = fobj['pfns'][0]['pfn']
                if flocation == "local":
                    # local copy to cwd
                    if fpath[:7] == "file://":
                        sp = 7
                    else:
                        sp = 0
                    cmd = ["/bin/cp", fpath[sp:], "./"] # '7:', removing the `file://` protocol by indexing counts
                else: # ipac
                    # remote download to cwd
                    cmd = ["/bin/curl", "-o", fname, f"'{fpath}'"]
                pre_exec.append(cmd)
            if ftype == "output":
                outputs.append(fname)
                if v['registerReplica']:
                    self.files[fname] = {'pfns': [{'site': 'local', 'pfn': f'file://${cid}/{fname}'}]}
            else:
                inputs.append(fname)
        return {'inputs': inputs,
                'outputs': outputs,
                'pre_exec': pre_exec}


    def retrieve_cmd_info(self, cid):
        t_exec = self.get_cmd(cid)
        args = self.get_arguments(cid)
        tmp = self.get_data(cid)
        return {'exec': t_exec, 'args': args, 'inputs': tmp['inputs'], 'outputs':
                tmp['outputs'], 'pre_exec': tmp['pre_exec']}


    def update_id_map_to_pst(self, t_info, m_info):
        pre_exec = t_info['pre_exec']
        for cmd in pre_exec:
            idx = 0
            for element in cmd:
                if element[0] == "$":
                    idname, rest = element.split("/",1)
                    tmp = m_info[idname[1:]]
                    cmd[idx] = "%s/%s" % (tmp, rest)
                idx += 1

    def convert_to_pst(self):
        pst = {}
        t_cnt = 0
        id_map_to_pst = {}
        for k, v in self.ordered_by_val.items():
            t_info = self.retrieve_cmd_info(k)
            s_name = 'stage_%s' % v
            t_name = 'task_%s' % t_cnt
            id_map_to_pst[k] = f'$pipeline_0_{s_name}_{t_name}'
            self.update_id_map_to_pst(t_info, id_map_to_pst)
            task = { t_name : t_info }
            if s_name in pst:
                pst[s_name].append(task)
            else:
                pst[s_name] = [task]
            t_cnt += 1

        pprint(pst, indent=4)
        self.pst = pst


    def save_pst_yml(self):
        outfname = self.args.radical_pst_yml
        if outfname:
            with open(outfname, 'w') as outfile:
                yaml.dump(self.pst, outfile, default_flow_style=False)


    def save_shell_script(self):
        outfname = self.args.output
        if outfname:
            with open(outfname, 'w') as outfile:
                lines = []
                for k, v in self.ordered_by_val.items():
                    t_info = self.retrieve_cmd_info(k)
                    for l in t_info['pre_exec']:
                        outfile.write(" ".join(l) + "\n")
                    line = "%s %s " % (t_info['exec'], " ".join(t_info['args']))
                    outfile.write(f"{line}\n")


if __name__ == "__main__":
    obj = Conversion()
    obj.user_input()
    obj.import_dax()
    obj.get_jdep()
    obj.convert_to_pst()
    obj.save_pst_yml() if obj.args.radical_pst_yml else ''
    obj.save_shell_script()
