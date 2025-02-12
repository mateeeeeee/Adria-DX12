# Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
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

from profiler_report_types import *
import argparse
import sys
import os

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

def gen_depfile(target_file_path, buildroot):
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

    return '\n'.join(depfile_contents)

# target_file_path : the file being generated
# buildroot        : root directory of the build system; this prefix is removed from target_file_path to pacify ninja
# depfile_path     : the depfile to be written
def write_depfile(target_file_path, buildroot, depfile_path):
    with open(depfile_path, 'w', encoding='utf-8') as out_fd:
        depfile_str = gen_depfile(target_file_path, buildroot)
        out_fd.write(depfile_str)

#===============================================================================
# C++ header generation
#===============================================================================

def write_cpp_file(out_fd, report_definition):
    out_fd.write(r'''
    namespace {} {{
'''.format(report_definition.name))

    out_fd.write(r'''
        inline ReportDefinition GetReportDefinition()
        {''')

    # counters
    if len(report_definition.required_counters):
        out_fd.write(r'''
            static const char* const RequiredCounters[] = {
''')
        for counter in report_definition.required_counters:
            out_fd.write(r'''                "{}",
'''.format(counter))
        out_fd.write(r'''            };
''')

    # ratios
    if len(report_definition.required_ratios):
        out_fd.write(r'''
            static const char* const RequiredRatios[] = {
''')
        for ratio in report_definition.required_ratios:
            out_fd.write(r'''                "{}",
'''.format(ratio))
        out_fd.write(r'''            };
''')

    # throughputs
    if len(report_definition.required_throughputs):
        out_fd.write(r'''
            static const char* const RequiredThroughputs[] = {
''')
        for throughput in report_definition.required_throughputs:
            out_fd.write(r'''                "{}",
'''.format(throughput))
        out_fd.write(r'''            };
''')

    # html template
    assert(len(report_definition.html));
    out_fd.write(r'''
            static const unsigned char ReportContents[] = {''')
    barray = bytearray(report_definition.html, 'utf-8')
    formatted_string = []
    for index, b in enumerate(barray):
        if index % 20 == 0:
            formatted_string += '\n                '
        assert(b <= 0xFF)
        formatted_string.append('0x{:02x}, '.format(b))
    out_fd.write("".join(formatted_string))
    out_fd.write(r'''0x0
            };
''')

    out_fd.write(r'''
            ReportDefinition reportDefinition = {''')

    if len(report_definition.required_counters):
        out_fd.write(r'''
                RequiredCounters,
                sizeof(RequiredCounters) / sizeof(RequiredCounters[0]),''')
    else:
        out_fd.write(r'''
                nullptr,
                0,''')

    if len(report_definition.required_ratios):
        out_fd.write(r'''
                RequiredRatios,
                sizeof(RequiredRatios) / sizeof(RequiredRatios[0]),''')
    else:
        out_fd.write(r'''
                nullptr,
                0,''')

    if len(report_definition.required_throughputs):
        out_fd.write(r'''
                RequiredThroughputs,
                sizeof(RequiredThroughputs) / sizeof(RequiredThroughputs[0]),''')
    else:
        out_fd.write(r'''
                nullptr,
                0,''')
    out_fd.write(r'''
                (const char*)ReportContents
            };
            return reportDefinition;
        }
''')
    out_fd.write(r'''
    }} // namespace {}


'''.format(report_definition.name))

#===============================================================================
# Main
#===============================================================================

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate HTML report definition')
    parser.add_argument('--chip', type=str, required=True, help='chip name, e.g. tu10x')
    parser.add_argument('--outDir', type=str, required=True, help='output directory')
    parser.add_argument('--pypath', default=[], action='append', required=False, help="Python module paths.")
    parser.add_argument('--buildroot', default='', required=False, help="build root dir for depfile")
    parser.add_argument('--copyright', type=str, help="Copyright header.")

    args = parser.parse_args()
    sys.path.extend(args.pypath)
    sys.path.extend(".")
    chip = args.chip
    report_module_name = "report_" + args.chip
    try:
        report_module = __import__(report_module_name)
    except ImportError:
        raise ImportError('Module "{}" is not found, this could happen due to invalid chip name or insufficient --pypath.'.format(report_module_name))
    per_range_report_definition = report_module.get_per_range_report_definition()
    summary_report_definition = report_module.get_summary_report_definition()

    if not os.path.isdir(args.outDir):
        raise Exception('Invalid argument for --outDir: {}'.format(args.outDir))

    # Per-range report: debug html(this can be used for inspection, the debug mode also lists the metrics that are used by each table)
    range_debug_html_file_name = 'NvPerfReportDefinition{}_range_debug.html'.format(chip.upper())
    range_debug_html_file_path = os.path.join(args.outDir, range_debug_html_file_name)
    with open(range_debug_html_file_path, 'w', encoding='utf-8') as out_fd:
        out_fd.write(per_range_report_definition.html)

    # Summary report: debug html
    summary_debug_html_file_name = 'NvPerfReportDefinition{}_summary_debug.html'.format(chip.upper())
    summary_debug_html_file_path = os.path.join(args.outDir, summary_debug_html_file_name)
    with open(summary_debug_html_file_path, 'w', encoding='utf-8') as out_fd:
        out_fd.write(summary_report_definition.html)


    # CPP file
    cpp_file_name = 'NvPerfReportDefinition{}.h'.format(chip.upper())
    cpp_file_path = os.path.join(args.outDir, cpp_file_name)
    with open(cpp_file_path, 'w') as out_fd:
        if args.copyright:
            with open(args.copyright, 'r') as copyright_fd:
                out_fd.write(copyright_fd.read())

        out_fd.write(r'''
#pragma once

#include "NvPerfReportDefinition.h"

namespace nv {{ namespace perf {{ namespace {} {{
'''.format(args.chip))
        # per-range report
        write_cpp_file(out_fd, per_range_report_definition)
        # summary report
        write_cpp_file(out_fd, summary_report_definition)
        out_fd.write(r'''
} } }''')

    # Emit a single depfile using the C++ file as the representative output.
    write_depfile(cpp_file_path, args.buildroot, cpp_file_path + '.d')
