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

class DevicePropertiesGenerator(tables_common.DevicePropertiesGenerator):
    def __init__(gen):
        super().__init__()
        gen.l2cacheSizePerLts = 128

class TopThroughputsGenerator(tables_common.TopThroughputsGenerator):
    def __init__(gen):
        super().__init__()
        gen.rows += [
            gen.Row('Shader'      , '<a href="#SM-Instruction-Throughput">SM (Shader Cores)</a>'        , 'sm__throughput'      ),
            gen.Row('Memory'      , '<a href="#L1TEX-Throughput">L1TEX Cache</a>'                       , 'l1tex__throughput'   ),
            gen.Row('Memory'      , '<a href="#L2-Sector-Traffic-By-Memory-Aperture-Short">L2 Cache</a>', 'lts__throughput'     ),
            gen.Row('Memory'      , '<a href="#Main-Memory-Throughput">DRAM</a>'                        , 'dram__throughput'    ),
            gen.Row('Memory'      , '<a href="#Main-Memory-Throughput">PCIe</a>'                        , 'pcie__throughput'    ),
            gen.Row('World Pipe'  , '<a href="#Primitive-Data-Flow">PDA Index Fetch</a>'                , 'pda__throughput'     ),
            gen.Row('World Pipe'  , '<a href="#Primitive-Data-Flow">Vertex Attr. Fetch</a>'             , 'vaf__throughput'     ),
            gen.Row('World Pipe'  , '<a href="#Primitive-Data-Flow">Primitive Engine</a>'               , 'pes__throughput'     ),
            gen.Row('Screen Pipe' , '<a href="#Raster-Data-Flow">RASTER</a>'                            , 'raster__throughput'  ),
            gen.Row('Screen Pipe' , '<a href="#Raster-Data-Flow">PROP (Pre-ROP)</a>'                    , 'prop__throughput'    ),
            gen.Row('Screen Pipe' , '<a href="#Raster-Data-Flow">ZROP (Depth-Test)</a>'                 , 'zrop__throughput'    ),
            gen.Row('Screen Pipe' , '<a href="#Raster-Data-Flow">CROP (Color Blend)</a>'                , 'crop__throughput'    ),
        ]
        gen.required_throughputs += [
            'crop__throughput',
            'dram__throughput',
            'l1tex__throughput',
            'lts__throughput',
            'pcie__throughput',
            'pda__throughput',
            'pes__throughput',
            'prop__throughput',
            'raster__throughput',
            'sm__throughput',
            'vaf__throughput',
            'zrop__throughput',
        ]

class SmThroughputsGenerator_tu10x(tables_common.SmThroughputsGenerator):
    def __init__(gen):
        super().__init__()
        gen.pipes += [
            gen.Pipe('adu'          , 'Computed branches and indexed constants'),
            gen.Pipe('alu'          , 'INT32 except multiply; FP32 comparison'),
            gen.Pipe('cbu'          , 'Divergent branches and control flow'),
            gen.Pipe('fma'          , 'FP32 mul/add and INT32 multiply'),
            gen.Pipe('fp16'         , 'FP16 mul/add'),
            gen.Pipe('fp64'         , 'FP64 mul/add'),
            gen.Pipe('ipa'          , 'Pixel shader attribute interpolation'),
            gen.Pipe('lsu'          , 'Global, local, shared memory, and misc'),
            gen.Pipe('shared'       , 'Shared Pipe Dispatch (FP16,Tensor)', 'sm__pipe_shared_cycles_active', hasInstExecuted=False),
            gen.Pipe('tensor'       , 'Tensor matrix multiply (FP16, INT8/4/1)'),
            gen.Pipe('tex'          , 'Texture and surface memory'),
            gen.Pipe('uniform'      , 'Warp-level scalar operations'),
            gen.Pipe('xu'           , 'Transcendentals and float/int conversion'),
        ]
        for pipe in gen.pipes:
            gen.required_counters.extend(pipe.get_counter_names())

class SmThroughputsGenerator_tu11x(tables_common.SmThroughputsGenerator):
    def __init__(gen):
        super().__init__()
        gen.pipes = []
        gen.pipes += [
            gen.Pipe('adu'          , 'Computed branches and indexed constants'),
            gen.Pipe('alu'          , 'INT32 except multiply; FP32 comparison'),
            gen.Pipe('cbu'          , 'Divergent branches and control flow'),
            gen.Pipe('fma'          , 'FP32 mul/add and INT32 multiply'),
            gen.Pipe('fp16'         , 'FP16 mul/add'),
            gen.Pipe('fp64'         , 'FP64 mul/add'),
            gen.Pipe('ipa'          , 'Pixel shader attribute interpolation'),
            gen.Pipe('lsu'          , 'Global, local, shared memory, and misc'),
            gen.Pipe('tensor'       , 'Tensor matrix multiply (FP16, INT8/4/1)'),
            gen.Pipe('tex'          , 'Texture and surface memory'),
            gen.Pipe('uniform'      , 'Warp-level scalar operations'),
            gen.Pipe('xu'           , 'Transcendentals and float/int conversion'),
        ]
        for pipe in gen.pipes:
            gen.required_counters.extend(pipe.get_counter_names())

class SmInstExecutedGenerator(tables_common.SmInstExecutedGenerator):
    def __init__(gen):
        super().__init__()
        gen.pipes = [
            gen.Pipe('total'        , 'All instructions', True),
            gen.Pipe('adu'          , 'Computed branches and indexed constants'),
            gen.Pipe('alu'          , 'INT32 except multiply; FP32 comparison', True),
            gen.Pipe('cbu'          , 'Divergent branches and control flow'),
            gen.Pipe('fma'          , 'FP32 mul/add and INT32 multiply', True),
            gen.Pipe('fp16'         , 'FP16 mul/add', True),
            gen.Pipe('fp64'         , 'FP64 mul/add', True),
            gen.Pipe('ipa'          , 'Pixel shader attribute interpolation', True),
            gen.Pipe('lsu'          , 'Global, local, shared memory, and misc', True),
            gen.Pipe('tensor'       , 'Tensor matrix multiply (FP16, INT8/4/1)'),
            gen.Pipe('tex'          , 'Texture and surface memory'),
            gen.Pipe('uniform'      , 'Warp-level scalar operations'),
            gen.Pipe('xu'           , 'Transcendentals and float/int conversion', True),
        ]
        for pipe in gen.pipes:
            gen.required_counters.extend(pipe.get_counter_names())

class L2TrafficByMemoryApertureShortBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow)
        gen.name = 'L2SectorTrafficBreakdownByMemoryApertureShort'
        gen.table_id = 'L2-Sector-Traffic-By-Memory-Aperture-Short'
        gen.column_names = [
            ('Memory Aperture', 'To Memory'),
            ('Op', 'Op'),
        ]
        gen.workflow += ' This table decomposes L2 bandwidth to each destination, per operation. A <a href="#L2-Sector-Traffic-By-Memory-Aperture">more detailed version of this table</a> can be found below.'

        gen.nodes = [
            gen.Node('DRAM' , ['lts__average_t_sector_aperture_device'], [
                gen.Node('Reads',      ['lts__average_t_sector_aperture_device_op_read'],  []),
                gen.Node('Writes',     ['lts__average_t_sector_aperture_device_op_write'], []),
                gen.Node('Atomics',    ['lts__average_t_sector_aperture_device_op_atom'], []),
                gen.Node('Reductions', ['lts__average_t_sector_aperture_device_op_red'],  []),
            ]),
            gen.Node('System Memory', ['lts__average_t_sector_aperture_sysmem'], [
                gen.Node('Reads',      ['lts__average_t_sector_aperture_sysmem_op_read'],  []),
                gen.Node('Writes',     ['lts__average_t_sector_aperture_sysmem_op_write'], []),
                gen.Node('Atomics',    ['lts__average_t_sector_aperture_sysmem_op_atom'], []),
                gen.Node('Reductions', ['lts__average_t_sector_aperture_sysmem_op_red'],  []),
            ]),
            gen.Node('Peer Memory', ['lts__average_t_sector_aperture_peer'], [
                gen.Node('Reads',      ['lts__average_t_sector_aperture_peer_op_read'],  []),
                gen.Node('Writes',     ['lts__average_t_sector_aperture_peer_op_write'], []),
                gen.Node('Atomics',    ['lts__average_t_sector_aperture_peer_op_atom'], []),
                gen.Node('Reductions', ['lts__average_t_sector_aperture_peer_op_red'],  []),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class L2TrafficBySrcBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow)
        gen.name = 'L2SectorTrafficBreakdownBySource'
        gen.table_id = 'L2-Sector-Traffic-By-Source'
        gen.column_names = [
            ('Source Breakdown', 'From Source'),
            ('Unit Breakdown', 'From Unit'),
            ('Memory Aperture', 'To Memory'),
            ('Op', 'Op'),
        ]
        gen.workflow += ' This table decomposes L2 bandwidth from each source unit, to each destination, per operation. See also: these tables that prioritize <a href="#L2-Sector-Traffic-By-Memory-Aperture">destination Memory Aperture</a> and <a href="#L2-Sector-Traffic-By-Operation">Operation</a>.'

        gen.nodes = [
            gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc'], [
                gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex'], [
                    gen.Node('DRAM' , ['lts__average_t_sector_srcunit_tex_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_tex_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_tex_aperture_device_op_write'], []),
                        gen.Node('Atomics',    ['lts__average_t_sector_srcunit_tex_aperture_device_op_atom'], []),
                        gen.Node('Reductions', ['lts__average_t_sector_srcunit_tex_aperture_device_op_red'],  []),
                    ]),
                    gen.Node('Peer Memory', ['lts__average_t_sector_srcunit_tex_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_tex_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_tex_aperture_peer_op_write'], []),
                        gen.Node('Atomics',    ['lts__average_t_sector_srcunit_tex_aperture_peer_op_atom'], []),
                        gen.Node('Reductions', ['lts__average_t_sector_srcunit_tex_aperture_peer_op_red'],  []),
                    ]),
                    gen.Node('System Memory', ['lts__average_t_sector_srcunit_tex_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_write'], []),
                        gen.Node('Atomics',    ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_atom'], []),
                        gen.Node('Reductions', ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_red'],  []),
                    ]),
                ]),
                gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe'], [
                    gen.Node('DRAM' , ['lts__average_t_sector_srcunit_pe_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_pe_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_pe_aperture_device_op_write'], []),
                    ]),
                    gen.Node('Peer Memory', ['lts__average_t_sector_srcunit_pe_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_pe_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_pe_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory', ['lts__average_t_sector_srcunit_pe_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_pe_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_pe_aperture_sysmem_op_write'], []),
                    ]),
                ]),
                gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother'], [
                    gen.Node('DRAM' , ['lts__average_t_sector_srcunit_gpcother_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_gpcother_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_gpcother_aperture_device_op_write'], []),
                    ]),
                    gen.Node('Peer Memory', ['lts__average_t_sector_srcunit_gpcother_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_gpcother_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_gpcother_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory', ['lts__average_t_sector_srcunit_gpcother_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_gpcother_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_gpcother_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
            gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp'], [
                gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop'], [
                    gen.Node('DRAM' , ['lts__average_t_sector_srcunit_zrop_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_zrop_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_zrop_aperture_device_op_write'], []),
                    ]),
                    gen.Node('Peer Memory' , ['lts__average_t_sector_srcunit_zrop_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_zrop_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_zrop_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory' , ['lts__average_t_sector_srcunit_zrop_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_zrop_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_zrop_aperture_sysmem_op_write'], []),
                    ]),
                ]),
                gen.Node('CROP', ['lts__average_t_sector_srcunit_crop'], [
                    gen.Node('DRAM' , ['lts__average_t_sector_srcunit_crop_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_crop_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_crop_aperture_device_op_write'], []),
                    ]),
                    gen.Node('Peer Memory' , ['lts__average_t_sector_srcunit_crop_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_crop_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_crop_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory' , ['lts__average_t_sector_srcunit_crop_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_crop_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_crop_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
            gen.Node('HUB Units', [], [ # lts__average_t_sector_srcnode_fbp is buggy, use sum of children instead
                gen.Node('all HUB Units', [], [ # lts__average_t_sector_srcnode_fbp is buggy, use sum of children instead
                    gen.Node('DRAM' , ['lts__average_t_sector_srcnode_hub_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcnode_hub_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcnode_hub_aperture_device_op_write'], []),
                    ]),
                    gen.Node('Peer Memory', ['lts__average_t_sector_srcnode_hub_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcnode_hub_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcnode_hub_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory', ['lts__average_t_sector_srcnode_hub_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcnode_hub_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcnode_hub_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class L2TrafficByMemoryApertureBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow)
        gen.name = 'L2SectorTrafficBreakdownByMemoryAperture'
        gen.table_id = 'L2-Sector-Traffic-By-Memory-Aperture'
        gen.column_names = [
            ('Memory Aperture', 'To Memory'),
            ('Source Breakdown', 'From Source'),
            ('Unit Breakdown', 'From Unit'),
            ('Op', 'Op'),
        ]
        gen.workflow += ' This is an extended breakdown of <a href="#L2-Sector-Traffic-By-Memory-Aperture-Short">L2 Traffic by destination</a>. It decomposes L2 bandwidth to each destination, from each source unit, per operation.'

        gen.nodes = [
            gen.Node('DRAM' , ['lts__average_t_sector_aperture_device'], [
                gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_device'], [
                    gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_tex_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_tex_aperture_device_op_write'], []),
                        gen.Node('Atomics',    ['lts__average_t_sector_srcunit_tex_aperture_device_op_atom'], []),
                        gen.Node('Reductions', ['lts__average_t_sector_srcunit_tex_aperture_device_op_red'],  []),
                    ]),
                    gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_pe_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_pe_aperture_device_op_write'], []),
                    ]),
                    gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_gpcother_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_gpcother_aperture_device_op_write'], []),
                    ]),
                ]),
                gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_aperture_device'], [
                    gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_zrop_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_zrop_aperture_device_op_write'], []),
                    ]),
                    gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_crop_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_crop_aperture_device_op_write'], []),
                    ]),
                ]),
                gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_device'], [
                    gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_device'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcnode_hub_aperture_device_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcnode_hub_aperture_device_op_write'], []),
                    ]),
                ]),
            ]),
            gen.Node('Peer Memory', ['lts__average_t_sector_aperture_peer'], [
                gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_peer'], [
                    gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_tex_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_tex_aperture_peer_op_write'], []),
                        gen.Node('Atomics',    ['lts__average_t_sector_srcunit_tex_aperture_peer_op_atom'], []),
                        gen.Node('Reductions', ['lts__average_t_sector_srcunit_tex_aperture_peer_op_red'],  []),
                    ]),
                    gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_pe_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_pe_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_gpcother_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_gpcother_aperture_peer_op_write'], []),
                    ]),
                ]),
                gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_aperture_peer'], [
                    gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_zrop_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_zrop_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_crop_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_crop_aperture_peer_op_write'], []),
                    ]),
                ]),
                gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_peer'], [
                    gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_peer'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcnode_hub_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcnode_hub_aperture_peer_op_write'], []),
                    ]),
                ]),
            ]),
            gen.Node('System Memory', ['lts__average_t_sector_aperture_sysmem'], [
                gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_sysmem'], [
                    gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_write'], []),
                        gen.Node('Atomics',    ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_atom'], []),
                        gen.Node('Reductions', ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_red'],  []),
                    ]),
                    gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_pe_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_pe_aperture_sysmem_op_write'], []),
                    ]),
                    gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_gpcother_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_gpcother_aperture_sysmem_op_write'], []),
                    ]),
                ]),
                gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_aperture_sysmem'], [
                    gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_zrop_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_zrop_aperture_sysmem_op_write'], []),
                    ]),
                    gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcunit_crop_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcunit_crop_aperture_sysmem_op_write'], []),
                    ]),
                ]),
                gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_sysmem'], [
                    gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_sysmem'], [
                        gen.Node('Reads',      ['lts__average_t_sector_srcnode_hub_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['lts__average_t_sector_srcnode_hub_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class L2TrafficByOperationBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow)
        gen.name = 'L2SectorTrafficBreakdownByOperation'
        gen.table_id = 'L2-Sector-Traffic-By-Operation'
        gen.column_names = [
            ('Op', 'Op'),
            ('Memory Aperture', 'To Memory'),
            ('Source Breakdown', 'From Source'),
            ('Unit Breakdown', 'From Unit'),
        ]
        gen.workflow += ' This table decomposes L2 bandwidth per operation, to each destination, from each source unit.'

        gen.nodes = [
            gen.Node('Reads',      ['lts__average_t_sector_op_read'],  [
                gen.Node('DRAM', ['lts__average_t_sector_aperture_device_op_read'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_device_op_read'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_device_op_read'], []),
                        gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_aperture_device_op_read'], []),
                        gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother_aperture_device_op_read'], []),
                    ]),
                    gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_aperture_device_op_read'], [
                        gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_aperture_device_op_read'], []),
                        gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_aperture_device_op_read'], []),
                    ]),
                    gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_device_op_read'], [
                        gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_device_op_read'], []),
                    ]),
                ]),
                gen.Node('Peer Memory', ['lts__average_t_sector_aperture_peer_op_read'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_peer_op_read'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_peer_op_read'], []),
                        gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_aperture_peer_op_read'], []),
                        gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother_aperture_peer_op_read'], []),
                    ]),
                    gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_aperture_peer_op_read'], [
                        gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_aperture_peer_op_read'], []),
                        gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_aperture_peer_op_read'], []),
                    ]),
                    gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_peer_op_read'], [
                        gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_peer_op_read'], []),
                    ]),
                ]),
                gen.Node('System Memory', ['lts__average_t_sector_aperture_sysmem_op_read'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_sysmem_op_read'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_read'], []),
                        gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_aperture_sysmem_op_read'], []),
                        gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother_aperture_sysmem_op_read'], []),
                    ]),
                    gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_aperture_sysmem_op_read'], [
                        gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_aperture_sysmem_op_read'], []),
                        gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_aperture_sysmem_op_read'], []),
                    ]),
                    gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_sysmem_op_read'], [
                        gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_sysmem_op_read'], []),
                    ]),
                ]),
            ]),
            gen.Node('Writes',     ['lts__average_t_sector_op_write'], [
                gen.Node('DRAM', ['lts__average_t_sector_aperture_device_op_write'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_device_op_write'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_device_op_write'], []),
                        gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_aperture_device_op_write'], []),
                        gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother_aperture_device_op_write'], []),
                    ]),
                    gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_aperture_device_op_write'], [
                        gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_aperture_device_op_write'], []),
                        gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_aperture_device_op_write'], []),
                    ]),
                    gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_device_op_write'], [
                        gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_device_op_write'], []),
                    ]),
                ]),
                gen.Node('Peer Memory', ['lts__average_t_sector_aperture_peer_op_write'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_peer_op_write'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_peer_op_write'], []),
                        gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_aperture_peer_op_write'], []),
                        gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_aperture_peer_op_write'], [
                        gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_aperture_peer_op_write'], []),
                        gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_peer_op_write'], [
                        gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_peer_op_write'], []),
                    ]),
                ]),
                gen.Node('System Memory', ['lts__average_t_sector_aperture_sysmem_op_write'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_sysmem_op_write'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_write'], []),
                        gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_aperture_sysmem_op_write'], []),
                        gen.Node('other GPC units', ['lts__average_t_sector_srcunit_gpcother_aperture_sysmem_op_write'], []),
                    ]),
                    gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_aperture_sysmem_op_write'], [
                        gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_aperture_sysmem_op_write'], []),
                        gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_aperture_sysmem_op_write'], []),
                    ]),
                    gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_sysmem_op_write'], [
                        gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
            gen.Node('Atomics',    ['lts__average_t_sector_op_atom'], [
                gen.Node('DRAM', ['lts__average_t_sector_aperture_device_op_atom'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_device_op_atom'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_device_op_atom'], []),
                    ]),
                ]),
                gen.Node('Peer Memory', ['lts__average_t_sector_aperture_peer_op_atom'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_peer_op_atom'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_peer_op_atom'], []),
                    ]),
                ]),
                gen.Node('System Memory', ['lts__average_t_sector_aperture_sysmem_op_atom'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_sysmem_op_atom'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_atom'], []),
                    ]),
                ]),
            ]),
            gen.Node('Reductions', ['lts__average_t_sector_op_red'], [
                gen.Node('DRAM', ['lts__average_t_sector_aperture_device_op_red'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_device_op_red'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_device_op_red'], []),
                    ]),
                ]),
                gen.Node('Peer Memory', ['lts__average_t_sector_aperture_peer_op_red'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_peer_op_red'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_peer_op_red'], []),
                    ]),
                ]),
                gen.Node('System Memory', ['lts__average_t_sector_aperture_sysmem_op_red'], [
                    gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_aperture_sysmem_op_red'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_aperture_sysmem_op_red'], []),
                    ]),
                ]),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class RasterDataflowGenerator(tables_common.RasterDataflowGenerator):
    def __init__(gen):
        super().__init__('prop__prop2xbar_zrop_pixels_realtime', 'prop__prop2xbar_crop_pixels_realtime')
        gen.required_counters.extend([
            'prop__prop2xbar_zrop_pixels_realtime',
            'prop__prop2xbar_crop_pixels_realtime'
        ])

class SmResourceUsageGenerator(tables_common.SmResourceUsageGenerator):
    def __init__(gen):
        super().__init__()
        gen.rows = [
            #       resource                  total                                           gfx                                             vtg                                                                 ps                                                          cs
            gen.Row('Warps'                 , 'sm__warps_active'                            , 'NotApplicable'                               , 'tpc__warps_active_shader_vtg_realtime'                           , 'tpc__warps_active_shader_ps_realtime'                    , 'tpc__warps_active_shader_cs_realtime'                                ),
            gen.Row('Registers'             , 'tpc__sm_rf_registers_allocated'              , 'NotApplicable'                               , 'tpc__sm_rf_registers_allocated_shader_vtg'                       , 'tpc__sm_rf_registers_allocated_shader_ps'                , 'tpc__sm_rf_registers_allocated_shader_cs'                            ),
            gen.Row('Attr/ShMem'            , 'NotApplicable'                               , 'NotApplicable'                               , 'tpc__l1tex_sram_bytes_mem_untagged_data_shared_allocated_isbe'   , 'tpc__l1tex_sram_bytes_mem_untagged_data_tram_allocated'  , 'tpc__l1tex_sram_bytes_mem_untagged_data_shared_allocated_compute'    ),
            gen.Row('CTAs'                  , 'NotApplicable'                               , 'NotApplicable'                               , 'NotApplicable'                                                   , 'NotApplicable'                                           , 'sm__ctas_active'                                                     ),
        ]
        gen.required_counters = [
            'sm__ctas_active',
            'sm__warps_active',
            'tpc__warps_active_shader_cs_realtime',
            'tpc__warps_active_shader_ps_realtime',
            'tpc__warps_active_shader_vtg_realtime',
            'tpc__sm_rf_registers_allocated',
            'tpc__sm_rf_registers_allocated_shader_vtg',
            'tpc__sm_rf_registers_allocated_shader_ps',
            'tpc__sm_rf_registers_allocated_shader_cs',
            'tpc__l1tex_sram_bytes_mem_untagged_data_shared_allocated_isbe',
            'tpc__l1tex_sram_bytes_mem_untagged_data_tram_allocated',
            'tpc__l1tex_sram_bytes_mem_untagged_data_shared_allocated_compute',
        ]

class RangesSummaryGenerator(tables_common.RangesSummaryGenerator):
    def __init__(gen):
        super().__init__()
        gen.cols = [
            gen.Col('Duration ns'   , "getCounterValue('gpu__time_duration', 'avg')"                                                                            , 'format_avg'  , 'ra', "'gpu__time_duration.avg'"),
            gen.Col('GR Active%'    , "getCounterPct('gr__cycles_active', 'avg')"                                                                               , 'format_pct'  , 'ra', "'gr__cycles_active.avg.pct_of_peak_sustained_elapsed'"),
            gen.Col('3D?'           , "getCounterValue('fe__draw_count', 'sum') ? '&#x2713;' : ''"                                                              , ''            , 'ra', "'fe__draw_count.sum'"),
            gen.Col('Comp?'         , "getCounterValue('gr__dispatch_count', 'sum') ? '&#x2713;' : ''"                                                          , ''            , 'ra', "'gr__dispatch_count.sum'"),
            gen.Col('#WFI'          , "getCounterValue('fe__output_ops_type_bundle_cmd_go_idle', 'sum')"                                                        , 'format_sum'  , 'ra', "'fe__output_ops_type_bundle_cmd_go_idle.sum'"),
            gen.Col('#Prims'        , "getCounterValue('pda__input_prims', 'sum')"                                                                              , 'format_sum'  , 'ra', "'pda__input_prims.sum'"),
            gen.Col('#Pixels-Z'     , "getCounterValue('prop__prop2xbar_zrop_pixels_realtime', 'sum')"                                                          , 'format_sum'  , 'ra', "'prop__prop2xbar_zrop_pixels_realtime.sum'"),
            gen.Col('#Pixels-C'     , "getCounterValue('prop__prop2xbar_crop_pixels_realtime', 'sum')"                                                          , 'format_sum'  , 'ra', "'prop__prop2xbar_crop_pixels_realtime.sum'"),
            gen.Col('SM%'           , "getThroughputPct('sm__throughput')"                                                                                      , 'format_pct'  , 'ra', "getThroughputPctStr('sm__throughput')"),
            gen.Col('L1TEX%'        , "getThroughputPct('l1tex__throughput')"                                                                                   , 'format_pct'  , 'ra', "getThroughputPctStr('l1tex__throughput')"),
            gen.Col('L2%'           , "getThroughputPct('lts__throughput')"                                                                                     , 'format_pct'  , 'ra', "getThroughputPctStr('lts__throughput')"),
            gen.Col('DRAM%'         , "getThroughputPct('dram__throughput')"                                                                                    , 'format_pct'  , 'ra', "getThroughputPctStr('dram__throughput')"),
            gen.Col('PCIe%'         , "getThroughputPct('pcie__throughput')"                                                                                    , 'format_pct'  , 'ra', "getThroughputPctStr('pcie__throughput')"),
            gen.Col('PD%'           , "getThroughputPct('pda__throughput')"                                                                                     , 'format_pct'  , 'ra', "getThroughputPctStr('pda__throughput')"),
            gen.Col('PE%'           , "Math.max(getThroughputPct('vaf__throughput'), getThroughputPct('vpc__throughput'), getThroughputPct('pes__throughput'))" , 'format_pct'  , 'ra', "'max(' + getThroughputPctStr('vaf__throughput') + ', ' + getThroughputPctStr('vpc__throughput') + ', ' + getThroughputPctStr('pes__throughput') + ')'"),
            gen.Col('RSTR%'         , "getThroughputPct('raster__throughput')"                                                                                  , 'format_pct'  , 'ra', "getThroughputPctStr('raster__throughput')"),
            gen.Col('PROP%'         , "getThroughputPct('prop__throughput')"                                                                                    , 'format_pct'  , 'ra', "getThroughputPctStr('prop__throughput')"),
            gen.Col('ZROP%'         , "getThroughputPct('zrop__throughput')"                                                                                    , 'format_pct'  , 'ra', "getThroughputPctStr('zrop__throughput')"),
            gen.Col('CROP%'         , "getThroughputPct('crop__throughput')"                                                                                    , 'format_pct'  , 'ra', "getThroughputPctStr('crop__throughput')"),
        ]
        gen.required_counters = [
            'fe__draw_count',
            'fe__output_ops_type_bundle_cmd_go_idle',
            'gpu__time_duration',
            'gr__cycles_active',
            'gr__dispatch_count',
            'pda__input_prims',
            'prop__prop2xbar_crop_pixels_realtime',
            'prop__prop2xbar_zrop_pixels_realtime',
        ]
        gen.required_ratios = []
        gen.required_throughputs = [
            'crop__throughput',
            'dram__throughput',
            'l1tex__throughput',
            'lts__throughput',
            'pcie__throughput',
            'pda__throughput',
            'pes__throughput',
            'prop__throughput',
            'raster__throughput',
            'sm__throughput',
            'vaf__throughput',
            'vpc__throughput',
            'zrop__throughput',
        ]
