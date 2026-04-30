import pathlib
from wfcommons.wfchef.recipes import SeismologyRecipe
from wfcommons import WorkflowGenerator

num_tasks = [100, 250, 370, 800]

generator = WorkflowGenerator(SeismologyRecipe.from_num_tasks(250),input_file_size_factor=5, output_file_size_factor=5)
workflow = generator.build_workflow()
workflow.write_json(pathlib.Path('seismology-workflow.json'))