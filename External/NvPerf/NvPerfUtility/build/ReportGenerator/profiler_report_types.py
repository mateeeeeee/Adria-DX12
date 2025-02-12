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

class DataTable:
    def __init__(dtable, name, html, jsfunc, jscall, required_counters, required_ratios, required_throughputs, workflow):
        dtable.name = name
        dtable.html = html
        dtable.jsfunc = jsfunc
        dtable.jscall = jscall
        dtable.required_counters = required_counters
        dtable.required_ratios = required_ratios
        dtable.required_throughputs = required_throughputs
        dtable.workflow = workflow

class DataSection:
    def __init__(section, dtables, inter_table_spacing=True, title=None):
        section.dtables = dtables
        section.inter_table_spacing = inter_table_spacing
        section.title = title

class ReportDefinition:
    def __init__(rd, name, html, required_counters, required_ratios, required_throughputs):
        rd.name = name
        rd.html = html
        rd.required_counters = required_counters
        rd.required_ratios = required_ratios
        rd.required_throughputs = required_throughputs

def get_data_tables(sections):
    dtables = [dtable for section in sections for dtable in section.dtables]
    return dtables

def get_required_counters(sections):
    required_counters = set()
    dtables = get_data_tables(sections)
    for dtable in dtables:
        for counter in dtable.required_counters:
            required_counters.add(counter)
    required_counters = sorted(list(required_counters))
    return required_counters

def get_required_ratios(sections):
    required_ratios = set()
    dtables = get_data_tables(sections)
    for dtable in dtables:
        for ratio in dtable.required_ratios:
            required_ratios.add(ratio)
    required_ratios = sorted(list(required_ratios))
    return required_ratios

def get_required_throughputs(sections):
    required_throughputs = set()
    dtables = get_data_tables(sections)
    for dtable in dtables:
        for throughput in dtable.required_throughputs:
            required_throughputs.add(throughput)
    required_throughputs = sorted(list(required_throughputs))
    return required_throughputs