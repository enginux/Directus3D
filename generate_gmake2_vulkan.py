import os
import subprocess
# change working directory to script directory
os.chdir(os.path.dirname(__file__))
# run script
p = subprocess.Popen("build_scripts\\generate_project_files.py gmake2 vulkan", shell=True)
p.communicate() # wait for script to finish