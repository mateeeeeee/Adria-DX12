# Copyright 2021-2025 NVIDIA Corporation.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import sys
import os
import re
import glob

#===============================================================================
# Dep File generation, compatible with Make or Ninja build systems
#===============================================================================

def get_loaded_module_file_names():
    module_file_names = set()
    for name, module in sys.modules.items():
        path = getattr(module, "__file__", None)
        if not path:
            continue
        path = os.path.realpath(path)
        if path.endswith("<frozen>"):
            continue
        if not os.path.isabs(path):
            path = os.path.abspath(path)
        if not os.path.isfile(path):
            continue # filter out directories
        module_file_names.add(path)
    return sorted(list(module_file_names))

def gen_depfile(target_file_path, yaml_file_names, buildroot):
    target_path_canonicalized = os.path.normpath(os.path.normcase(target_file_path))
    buildroot_canonicalized = os.path.normpath(os.path.normcase(buildroot))
    target_path_final = target_file_path
    if target_path_canonicalized.startswith(buildroot_canonicalized):
        target_path_final = target_file_path[len(buildroot_canonicalized):]
    if target_path_final[0] in ('\\', '/'):
        target_path_final = target_path_final[1:]

    module_file_names = get_loaded_module_file_names()

    # escape space because otherwise it will be interpreted as a separator by ninja
    def escape_space(s):
        return s.replace(' ', '\\ ')

    depfile_contents = []
    depfile_contents.append(escape_space(target_path_final) + ':\\')
    
    for module_file_name in module_file_names:
        depfile_contents.append('\t' + escape_space(module_file_name) + ' \\')
    
    for file_name in yaml_file_names:
        depfile_contents.append('\t' + escape_space(file_name) + ' \\')

    return '\n'.join(depfile_contents)

# target_file_path : the file being generated
# buildroot        : root directory of the build system; this prefix is removed from target_file_path to pacify ninja
# yaml_file_names  : list of metric configuration yaml files that are embedded in the target header file
# depfile_path     : the depfile to be written
def write_depfile(target_file_path, buildroot, yaml_file_names, depfile_path):
    with open(depfile_path, 'w', encoding='utf-8') as out_fd:
        depfile_str = gen_depfile(target_file_path, yaml_file_names, buildroot)
        out_fd.write(depfile_str)

#===============================================================================
# C++ header generation
#===============================================================================

def write_cpp_file(out_fd, chip, yaml_file_names):

    out_fd.write(r'''
#pragma once

#include <cstddef>

namespace nv {{ namespace perf {{ namespace {} {{ namespace MetricConfigurations {{

    inline size_t GetMetricConfigurationsSize()
    {{
        return {};
    }}
'''.format(chip, len(yaml_file_names)))

    if (len(yaml_file_names) == 0):
        out_fd.write(r'''
    inline const char** GetMetricConfigurationsFileNames()
    {
        return nullptr;
    }
    
    inline const char** GetMetricConfigurations()
    {
        return nullptr;
    }
    
} } } } }''')

    else:
    
        out_fd.write(r'''
    inline const char** GetMetricConfigurationsFileNames()
    {
        static const char* yamlFileNames[] = {''')

        for index, file_name in enumerate(yaml_file_names):
            file_name_without_prefix=re.sub('.*NvPerfUtility/', 'NvPerfUtility/', file_name)
            out_fd.write(r'''
            "{}"{}'''.format(file_name_without_prefix, ", " if index < len(yaml_file_names)-1 else ""))

        out_fd.write(r'''
        };
        return yamlFileNames;
    }
    
    inline const char** GetMetricConfigurations()
    {''')
    
        yaml_contents_variable_list = "{"
        for index, file_name in enumerate(yaml_file_names):
            file_name_without_prefix=re.sub('.*NvPerfUtility/', 'NvPerfUtility/', file_name)
            with open(file_name, 'r') as fd:
                yaml_contents_variable_list += " (const char*)yamlContents{}".format(index)
                if index < len(yaml_file_names)-1:
                    yaml_contents_variable_list += ", "
                out_fd.write('''
        // {}
        static const unsigned char yamlContents{}[] = {{'''.format(file_name_without_prefix, index))

                barray = bytearray(fd.read(), 'utf-8')
                formatted_string = []
                for index, b in enumerate(barray):
                    if index % 20 == 0:
                        formatted_string += '\n            '
                    assert(b <= 0xFF)
                    formatted_string.append('0x{:02x}, '.format(b))
                
                formatted_string.append('0x{:02x}, '.format(ord('\n')))
                out_fd.write("".join(formatted_string))
    
                out_fd.write('''0x00
        };
''')
        yaml_contents_variable_list += " }"
        out_fd.write(r'''
        static const char* yamlContents[] = {};
        return (const char**) yamlContents;
    }}
    
}} }} }} }}'''.format(yaml_contents_variable_list))

#===============================================================================
# Main
#===============================================================================

if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='Generate metric configurations')
    parser.add_argument('--chip', type=str, required=True, help='chip name, e.g. tu10x')
    parser.add_argument('--outDir', type=str, required=True, help='output directory')
    parser.add_argument('--yamlPaths', default=[], nargs='+', required=True, help="Yaml MetricConfigurations paths")
    parser.add_argument('--pypath', default=[], action='append', required=False, help="Python module paths")
    parser.add_argument('--buildroot', default='', required=False, help="build root dir for depfile")
    parser.add_argument('--copyright', type=str, help="Copyright header")

    args = parser.parse_args()
    sys.path.extend(args.pypath)
    sys.path.extend(".")
    chip = args.chip

    if not os.path.isdir(args.outDir):
        raise Exception('Invalid argument for --outDir: {}'.format(args.outDir))
        
    yaml_file_names = []
    for yaml_path in args.yamlPaths:
        # Is allowed to be missing, but not allowed to be a file
        if os.path.isfile(yaml_path):
            raise Exception('Invalid argument for --yamlPaths: {}'.format(args.yamlPaths))
        elif not os.path.exists(yaml_path):
            continue
        
        for file_name in glob.iglob("{}/*.yaml".format(yaml_path)):
            if os.name == 'nt':
                # always use the slash as the path separator
                yaml_file_names.append(file_name.replace('\\', '/'))
            else:
                yaml_file_names.append(file_name)

    yaml_file_names = sorted(yaml_file_names)
    
    cpp_file_name = 'NvPerfMetricConfigurations{}.h'.format(chip.upper())
    cpp_file_path = os.path.join(args.outDir, cpp_file_name)
    with open(cpp_file_path, 'w') as out_fd:
        if args.copyright:
            with open(args.copyright, 'r') as copyright_fd:
                out_fd.write(copyright_fd.read())

        write_cpp_file(out_fd, chip, yaml_file_names)

    # Emit a single depfile using the C++ file as the representative output.
    write_depfile(cpp_file_path, args.buildroot, yaml_file_names, cpp_file_path + '.d')
