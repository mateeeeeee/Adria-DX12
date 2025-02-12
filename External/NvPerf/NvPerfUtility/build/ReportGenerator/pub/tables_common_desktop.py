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
    def __init__(gen):
        super().__init__()
        gen.rows.append(gen.Row('Memory Clock', "getCounterValue('dram__cycles_elapsed', 'avg')", 'dram_clks', 'dram__cycles_elapsed.avg', "getCounterValue('dram__cycles_elapsed', 'avg.per_second') * 2 * 1e-6", 'MT/s', 'dram__cycles_elapsed.avg.per_second * 2 / 1000000'))
        gen.required_counters.insert(0, 'dram__cycles_elapsed')

class MainMemoryGenerator:
    class Row:
        def __init__(row, name, bytes_counter, pct, pct_tooltip):
            row.name = name
            row.bytes_counter = bytes_counter
            row.pct = pct
            row.pct_tooltip = pct_tooltip

    def __init__(gen):
        gen.name = 'MainMemory'
        gen.rows = [
            gen.Row('DRAM Total'          , 'NotAvailable'          , "getThroughputPct('dram__throughput')"        , "getThroughputPctStr('dram__throughput')"       ),
            gen.Row('DRAM Reads'          , 'NotAvailable'          , "getThroughputPct('dram__read_throughput')"   , "getThroughputPctStr('dram__read_throughput')"  ),
            gen.Row('DRAM Writes'         , 'NotAvailable'          , "getThroughputPct('dram__write_throughput')"  , "getThroughputPctStr('dram__write_throughput')" ),
            gen.Row('PCIe Reads'          , 'pcie__read_bytes'      , "getCounterPct('pcie__read_bytes')"           , "getCounterPctStr('pcie__read_bytes')"          ),
            gen.Row('PCIe Writes'         , 'pcie__write_bytes'     , "getCounterPct('pcie__write_bytes')"          , "getCounterPctStr('pcie__write_bytes')"         ),
        ]
        gen.required_counters = [
            # 'dram__bytes',
            # 'dram__bytes_read',
            # 'dram__bytes_write',
            'pcie__read_bytes',
            'pcie__write_bytes',
        ]
        gen.required_ratios = []
        gen.required_throughputs = [
            'dram__read_throughput',
            'dram__throughput',
            'dram__write_throughput',
        ]
        gen.workflow = '''Main Memory Throughput:
Observe the bandwidth utilization per main memory region.
'''

    @property
    def html(gen):
        return '''
        <table style="display: inline-block; border: 1px solid;" id="Main-Memory-Throughput">
          <thead>
            <tr>
              <th colspan="3" class="la tablename">Main Memory Throughput</th>
              <th colspan="2" class="ca">%-of-peak</th>
            </tr>
            <tr>
              <th class="la">Name</th>
              <th class="ra">Bytes</th>
              <th class="ra">MiBytes/Sec</th>
              <th class="ra">%</th>
              <th class="base">││││▌││││▌││││▌││││▌</th>
            </tr>
          </thead>
          <tbody id="tbody_main_memory">
          </tbody>
        </table>
'''

    @property
    def jsfunc(gen):
        jsfunc_lines = []
        jsfunc_lines += ['''
      function tbody_MainMemory(tbody) {
        class Row {
          constructor(name, counterName, pct, pctTooltip) {
            this.name = name;
            if (counterName == 'NotAvailable') {
              this.bytes = new NotAvailable;
              this.bytesTooltip = '';
              this.mibytesPerSec = new NotAvailable;
              this.mibytesPerSecTooltip = '';
            } else {
              this.bytes = getCounterValue(counterName, 'sum');
              this.bytesTooltip = counterName + '.sum';
              this.mibytesPerSec = getCounterValue(counterName, 'sum.per_second') / 1024 / 1024;
              this.mibytesPerSecTooltip = counterName + '.sum.per_second / 1024 / 1024';
            }
            this.pct = pct;
            this.pctTooltip = pctTooltip;
          }
        }
        let rows = [];
''']
        for row in gen.rows:
            qname = "'{row.name}'".format(row=row)
            qbytes_counter = "'{row.bytes_counter}'".format(row=row)
            jsfunc_lines += ["        rows.push(new Row({qname:<30}, {qbytes_counter:<30}, {row.pct:<50}, {row.pct_tooltip:<70}));\n".format(**locals())]
        jsfunc_lines += ['''
        for (const row of rows) {
          var trow = document.createElement('tr');
          addCellSimple(trow, "la subhdr", row.name);
          addCellSimple(trow, "ra", format_sum(row.bytes), passThrough=false, formatMetricFormula(row.bytesTooltip));
          addCellSimple(trow, "ra", format_avg(row.mibytesPerSec), passThrough=false, formatMetricFormula(row.mibytesPerSecTooltip));
          addCellSimple(trow, "ra", format_pct(row.pct), passThrough=false, formatMetricFormula(row.pctTooltip));
          addCellSimple(trow, "la comp", toBarChart(row.pct, '█'), passThrough=false, formatMetricFormula(row.pctTooltip));
          tbody.appendChild(trow);
        }
      }
''']
        funcbody = ''.join(jsfunc_lines)
        return funcbody

    @property
    def jscall(gen):
        return '''tbody_MainMemory(document.getElementById('tbody_main_memory'));'''

    def make_data_table(gen):
        dtable = DataTable(gen.name, gen.html, gen.jsfunc, gen.jscall, gen.required_counters, gen.required_ratios, gen.required_throughputs, gen.workflow)
        return dtable
