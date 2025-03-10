# Created by serboba at 24.03.22
import os
from lxml import etree as ET


def fix_paths():

    envs = os.path.dirname(os.path.abspath(__file__)) + '/envs/'
    for folder in os.listdir(envs):
        urdf_folder = envs + folder + '/urdf/'
        for urdf in os.listdir(urdf_folder):
            urdf_name = urdf_folder + urdf
            mesh_path = envs + folder + '/meshes/'
            tree = ET.parse(urdf_name)
            for mesh in tree.findall('link/visual/geometry/mesh'):
                mesh_name = mesh.get('filename')
                second_last = mesh_name.split('/')[-2]
                if second_last == 'stl' or second_last == 'dae' or second_last == 'obj':
                    mesh_name = mesh_path + second_last + '/' + mesh_name.split('/')[-1]
                else:
                    mesh_name = mesh_path + mesh_name.split('/')[-1]
                mesh.set("filename", mesh_name)

            for mesh in tree.findall('link/collision/geometry/mesh'):
                mesh_name = mesh.get('filename')
                second_last = mesh_name.split('/')[-2]
                if second_last == 'stl' or second_last == 'dae' or second_last == 'obj':
                    mesh_name = mesh_path + second_last + '/' + mesh_name.split('/')[-1]
                else:
                    mesh_name = mesh_path + mesh_name.split('/')[-1]
                mesh.set("filename", mesh_name)

            tree.write(urdf_name)


fix_paths()

# def main():
#     fix_paths()
#
# if __name__ == '__main__':
#     main()