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
import pub.tables_common_desktop as tables_common
import pub.turing.tables_turing as tables_turing

def get_per_range_report_definition_impl(tables_turing, tables_common):
    sections = [
        DataSection([
            tables_turing.DevicePropertiesGenerator().make_data_table(),
            tables_common.ClocksGenerator().make_data_table(),
        ], inter_table_spacing=False),
        DataSection([
            tables_common.TopLevelStatsGenerator().make_data_table(),
            tables_turing.TopThroughputsGenerator().make_data_table(),
            tables_common.CacheHitRates().make_data_table(),
        ], title='Overview Section'),
        DataSection([
            tables_common.MainMemoryGenerator().make_data_table(),
            tables_turing.L2TrafficByMemoryApertureShortBreakdownGenerator(show_generic_workflow=True).make_data_table(),
            tables_turing.L2TrafficBySrcBreakdownGenerator(show_generic_workflow=False).make_data_table(),
            tables_common.L1TexThroughputsGenerator().make_data_table(),
            tables_common.L1TexTrafficBreakdownGenerator().make_data_table(),
        ], title='Memory Performance Section'),
        DataSection([
            tables_turing.SmThroughputsGenerator_tu10x().make_data_table(),
            tables_turing.SmInstExecutedGenerator().make_data_table(),
            tables_common.SmShaderExecutionGenerator().make_data_table(),
            tables_turing.SmResourceUsageGenerator().make_data_table(),
            tables_common.SmWarpLaunchStallsGenerator().make_data_table(),
            tables_common.WarpIssueStallsGenerator().make_data_table(),
        ], title='Shader Performance Section'),
        DataSection([
            tables_common.PrimitiveDataflowGenerator().make_data_table(),
            tables_turing.RasterDataflowGenerator().make_data_table(),
        ], title='3D Pipeline Section'),
        DataSection([
            tables_turing.L2TrafficByMemoryApertureBreakdownGenerator(show_generic_workflow=False).make_data_table(),
            tables_turing.L2TrafficByOperationBreakdownGenerator(show_generic_workflow=False).make_data_table(),
        ], title='Additional L2 Traffic Breakdowns Section'),
        DataSection([
            tables_common.AdditionalMetricsGenerator().make_data_table(),
            tables_common.AllCountersGenerator().make_data_table(),
            tables_common.AllRatiosGenerator().make_data_table(),
            tables_common.AllThroughputsGenerator().make_data_table(),
        ], title='Exhaustive Listings Section'),
    ]
    html = tables_common.generate_range_html_common(sections)
    required_counters = get_required_counters(sections)
    required_ratios = get_required_ratios(sections)
    required_throughputs = get_required_throughputs(sections)
    return ReportDefinition('PerRangeReport', html, required_counters, required_ratios, required_throughputs)

def get_per_range_report_definition():
    return get_per_range_report_definition_impl(tables_turing, tables_common)

def get_summary_report_definition_impl(tables_turing, tables_common):
    sections = [
        DataSection([
            tables_common.CollectionInfoGenerator().make_data_table(),
        ]),
        DataSection([
            tables_turing.RangesSummaryGenerator().make_data_table(),
        ], title='Summary of Measured Ranges'),
    ]
    html = tables_common.generate_summary_html_common(sections)
    required_counters = get_required_counters(sections)
    required_ratios = get_required_ratios(sections)
    required_throughputs = get_required_throughputs(sections)
    return ReportDefinition('SummaryReport', html, required_counters, required_ratios, required_throughputs)

def get_summary_report_definition():
    return get_summary_report_definition_impl(tables_turing, tables_common)