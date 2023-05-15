# DAG to Shell Script

The abstract workflow description can be translated into a shell script. You
may use the following script with the description YAML file(s), for example of
using Montage workflow from `examples/montage-workflow.yml` is shown below:

```
python dag_to_shell.py -w example/montage-workflow.yml -o montage_workflow.sh
```

This generates an output file named `montage_workflow.sh` and the content of
the file may look like:

```
$ more montage_workflow.sh
...
/bin/curl -o poss2ukstu_blue_001_001.fits 'http://archive.stsci.edu/cgi-bin/dss_search?v=poss2ukstu_blue&r=57.135432&d=23.600372&e=J2000&w=42.60&h=42.60&f=fits&c=gz'
/files0/projectname/username/Montage/bin/mProject -X poss2ukstu_blue_001_001.fits pposs2ukstu_blue_001_001.fits region-oversized.hdr
...
```

The workflow description can be provided in three individual YAML files, where
`workflow_yml` serves Site catalog, `replica_yml` serves Replica catalog, and
`transformation_yml` serves Transformation catalog based on [the official
documentation of
Pegasus](https://pegasus.isi.edu/documentation/user-guide/creating-workflows.html).

And they can be loaded:

```
python dag_to_shell.py --workflow_yml workflow.yml \
                        --replica_yml replicas.yml \
                        --transformation_yml transformations.yml \
                        --output montage_workflow.sh
```

This montage workflow will convert 23 inputs (replica catalog), 73 executables (transformation catalog), and 157 job dependencies (workflow catalog) into a shell script file.

```
>>> len(d['transformationCatalog']['transformations'])
73
>>> len(d['replicaCatalog']['replicas'])
23
>>> len(d['jobs'])
157
```


# DAG to RADICAL PST YAML

The script provides a way to convert the workflow abstraction into the
`radical_pst_yml` which takes a filename of RADICAL PST to store.

```
python dag_to_shell.py --workflow_yml workflow.yml \
                        --replica_yml replicas.yml \
                        --transformation_yml transformations.yml \
                        --radical_pst_yml radical_pst.yml
```

Or the single YAML file may represent all necessary workflow information to
convert, and you use the script like this:

```
python dag_to_shell.py -w example/montage-workflow.yml -p radical_pst.yml
```


and the expected output message:
```
{   'stage_1': [   {   'task_0': {   'args': [   '-X',
                                                 'poss2ukstu_blue_001_001.fits',
                                                 'pposs2ukstu_blue_001_001.fits',
                                                 'region-oversized.hdr'],
                                     'exec': '/files0/projectname/username/Montage/bin/mProject',
                                     'inputs': [   'region-oversized.hdr',
                                                   'poss2ukstu_blue_001_001.fits'],
                                     'outputs': [   'pposs2ukstu_blue_001_001_area.fits',
                                                    'pposs2ukstu_blue_001_001.fits'],
                                     'pre_exec': [   [   '/bin/cp',
                                                         '/files0/projectname/username/montage-workflow-v3/data/region-oversized.hdr',
                                                         './'],
                                                     [   '/bin/curl',
                                                         '-o',
                                                         'poss2ukstu_blue_001_001.fits',
                                                         "'http://archive.stsci.edu/cgi-bin/dss_search?v=poss2ukstu_blue&r=57.135432&d=23.600372&e=J2000&w=42
.60&h=42.60&f=fits&c=gz'"]]}},
                   {   'task_1': {   'args': [   '-X',
                                                 'poss2ukstu_blue_001_002.fits',
                                                 'pposs2ukstu_blue_001_002.fits',
                                                 'region-oversized.hdr'],
                                     'exec': '/files0/projectname/username/Montage/bin/mProject',
                                     'inputs': [   'poss2ukstu_blue_001_002.fits',
                                                   'region-oversized.hdr'],
                                     'outputs': [   'pposs2ukstu_blue_001_002_area.fits',
                                                    'pposs2ukstu_blue_001_002.fits'],
                                     'pre_exec': [   [   '/bin/curl',
                                                         '-o',
                                                         'poss2ukstu_blue_001_002.fits',
                                                         "'http://archive.stsci.edu/cgi-bin/dss_search?v=poss2ukstu_blue&r=57.137125&d=24.100351&e=J2000&w=42
.60&h=42.60&f=fits&c=gz'"],
                                                     [   '/bin/cp',
                                                         '/files0/projectname/username/montage-workflow-v3/data/region-oversized.hdr',
                                                         './']]}},


...(skipped)...

    'stage_2': [   {   'task_16': {   'args': [   '-d',
                                                  '-s',
                                                  '1-fit.000001.000003.txt',
                                                  'pposs2ukstu_blue_001_001.fits',
                                                  'pposs2ukstu_blue_001_003.fits',
                                                  '1-diff.000001.000003.fits',
                                                  'region-oversized.hdr'],
                                      'exec': '/files0/projectname/username/Montage/bin/mDiffFit',
                                      'inputs': [   'pposs2ukstu_blue_001_003_area.fits',
                                                    'pposs2ukstu_blue_001_001_area.fits',
                                                    'region-oversized.hdr',
                                                    'pposs2ukstu_blue_001_003.fits',
                                                    'pposs2ukstu_blue_001_001.fits'],
                                      'outputs': ['1-fit.000001.000003.txt'],
                                      'pre_exec': [   [   '/bin/cp',
                                                          '$pipeline_0_stage_1_task_2/pposs2ukstu_blue_001_003_area.fits',
                                                          './'],
                                                      [   '/bin/cp',
                                                          '$pipeline_0_stage_1_task_0/pposs2ukstu_blue_001_001_area.fits',
                                                          './'],
                                                      [   '/bin/cp',
                                                          '/files0/projectname/username/montage-workflow-v3/data/region-oversized.hdr',
                                                          './'],
                                                      [   '/bin/cp',
                                                          '$pipeline_0_stage_1_task_2/pposs2ukstu_blue_001_003.fits',
                                                          './'],
                                                      [   '/bin/cp',
                                                          '$pipeline_0_stage_1_task_0/pposs2ukstu_blue_001_001.fits',
                                                          './']]}},
...(skipped)...

    'stage_8': [   {   'task_156': {   'args': [   '-ct',
                                                   '1',
                                                   '-gray',
                                                   '1-mosaic.fits',
                                                   '-1s',
                                                   'max',
                                                   'gaussian',
                                                   '-png',
                                                   '1-mosaic.png'],
                                       'exec': '/files0/projectname/username/Montage/bin/mViewer',
                                       'inputs': ['1-mosaic.fits'],
                                       'outputs': ['1-mosaic.png'],
                                       'pre_exec': [   [   '/bin/cp',
                                                           '$pipeline_0_stage_7_task_155/1-mosaic.fits',
                                                           './']]}}]}
```
