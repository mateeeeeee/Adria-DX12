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

from pub.tables_common_base import *

class ClocksGenerator(ClocksGeneratorBase):
    pass

class SocMemoryTrafficBreakdownGenerator:

    class Node:
        def __init__(node, label, metrics, children):
            node.label = label
            node.metrics = metrics
            if len(node.metrics) == 0:
                for child in children:
                    node.metrics.extend(child.metrics)
            node.children = children

        def to_javascript(node, base_indent):
            def to_js_recursive(cur_node, indent):
                str = ''
                str += '  ' * indent
                str += "new Node('{cur_node.label}', [".format(cur_node=cur_node)
                for metric in cur_node.metrics:
                    str += "'" + metric + "', "
                if len(cur_node.children):
                    str += '], [\n'
                    for child in cur_node.children:
                        str += to_js_recursive(child, indent + 1)
                    str += '  ' * indent + ']),\n'
                else:
                    str += '], []),\n'
                return str
            return to_js_recursive(node, base_indent)

    def get_required_counters(gen):
        def _get_required_counters_recursive(nodes):
            required_counters = []
            for node in nodes:
                required_counters += node.metrics
                required_counters += _get_required_counters_recursive(node.children)
            return required_counters
        required_counters = _get_required_counters_recursive(gen.nodes)
        return required_counters


    def __init__(gen):
        gen.name = 'SocMemoryThroughput'
        gen.table_id = 'Soc-Memory-Throughput'
        gen.column_names = [
            ('', 'Source'),
            ('Op', 'Op'),
        ]
        gen.nodes = [
            gen.Node('CPU', [], [
                gen.Node('Read', ['mcc__dram_throughput_srcnode_cpu_op_read'], []),
                gen.Node('Write', ['mcc__dram_throughput_srcnode_cpu_op_write'], []),
            ]),
            gen.Node('GPU', [], [
                gen.Node('Read', ['mcc__dram_throughput_srcnode_gpu_op_read'], []),
                gen.Node('Write', ['mcc__dram_throughput_srcnode_gpu_op_write'], []),
            ]),
            gen.Node('Other IPs', [], [
                gen.Node('Read', ['mcc__dram_throughput_srcnode_dbb_op_read'], []),
                gen.Node('Write', ['mcc__dram_throughput_srcnode_dbb_op_write'], []),
            ]),
        ]
        gen.required_counters = gen.get_required_counters()
        gen.required_ratios = []
        gen.required_throughputs = []
        gen.workflow = '''SoC Memory Throughput:
Observe the bandwidth utilization per SoC memory source and operation.
'''

    @property
    def html(gen):
        html_lines = ['''
        <table style="display: inline-block; border: 1px solid;" id="{table_id}">
          <thead>
            <tr>'''.format(table_id=gen.table_id)]
        for index, column in enumerate(gen.column_names):
          if not index:
            html_lines += ['''
              <th colspan="1" class="la tablename">Soc Memory Throughput</th>
              <th colspan="2" class="ca">%-of-Peak</th>''']
          else:
            html_lines += ['''
              <th colspan="1" class="ca">{column[0]}</th>
              <th colspan="2" class="ca">%-of-Peak</th>
'''.format(column=column)]
        html_lines += ['''
            </tr>
            <tr>''']
        for index, column in enumerate(gen.column_names):
          html_lines += ['''
              <th class="la">{column[1]}</th>
              <th class="ca">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>'''.format(column=column)]
        html_lines += ['''
            </tr>
          </thead>
          <tbody id="tbody_{table_id_us}">
          </tbody>
        </table>
'''.format(table_id_us=gen.table_id.replace('-', '_').lower())]
        return ''.join(html_lines)

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_{name}(tbody) {{
        class Node {{
          constructor(label, metrics, children) {{
            this.label = label;
            this.metrics = metrics;
            this.children = children;
            this.bytePct = this.metrics.reduce((acc, metric) => acc + getCounterValue(metric, 'avg.pct_of_peak_sustained_elapsed'), 0);
            this.rowspan = Math.max(1, children.reduce((acc, child) => acc + child.rowspan, 0));
          }}
        }}
        let topLevelNodes = [
'''.format(name=gen.name)]
        for node in gen.nodes:
            jsfunc_lines += [node.to_javascript(5)]
        jsfunc_lines += ['''
        ];
        function generateTableRecursive(trow_init, nodes) {
          let trow = trow_init;
          for (let ii = 0; ii < nodes.length; ++ii) {
            if (ii > 0) {
              trow = document.createElement('tr');
              tbody.appendChild(trow);
            }
            let node = nodes[ii];
  
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"la subhdr"}, node.label, passThrough=true);
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"ra"}, format_pct(node.bytePct));
            addCellAttr(trow, {'rowspan':node.rowspan, 'class':"la comp"}, toBarChart(node.bytePct, '█'));

            generateTableRecursive(trow, node.children);
          }
        }
        let trowFirst = document.createElement('tr');
        tbody.appendChild(trowFirst);
        generateTableRecursive(trowFirst, topLevelNodes);
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_{name}(document.getElementById('tbody_{table_id}'));'''.format(name=gen.name, table_id=gen.table_id.replace('-', '_').lower())

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable