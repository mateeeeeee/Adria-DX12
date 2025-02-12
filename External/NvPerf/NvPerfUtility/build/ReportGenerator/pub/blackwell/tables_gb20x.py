# Copyright 2014-2024 NVIDIA Corporation.  All rights reserved.
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

class VidL2TrafficByMemoryApertureShortBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow, l2_type_str='Vid')
        gen.name = 'VidL2SectorTrafficBreakdownByMemoryApertureShort'
        gen.table_id = 'VidL2-Sector-Traffic-By-Memory-Aperture-Short'
        gen.column_names = [
            ('Memory Aperture', 'To Memory'),
            ('Op', 'Op'),
        ]
        gen.workflow += ' This table decomposes VidL2 bandwidth to each destination, per operation. A <a href="#VidL2-Sector-Traffic-By-Memory-Aperture">more detailed version of this table</a> can be found below.'

        gen.nodes = [
            gen.Node('DRAM' , [], [
                gen.Node('Reads',      ['lts__average_t_sector_op_read'],  []),
                gen.Node('Writes',     ['lts__average_t_sector_op_write'], []),
                gen.Node('Atomics',    ['lts__average_t_sector_op_atom'], []),
                gen.Node('Reductions', ['lts__average_t_sector_op_red'],  []),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class SysL2TrafficByMemoryApertureShortBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow, l2_type_str='Sys')
        gen.name = 'SysL2SectorTrafficBreakdownByMemoryApertureShort'
        gen.table_id = 'SysL2-Sector-Traffic-By-Memory-Aperture-Short'
        gen.column_names = [
            ('Memory Aperture', 'To Memory'),
            ('Op', 'Op'),
        ]
        gen.workflow += ' This table decomposes SysL2 bandwidth to each destination, per operation. A <a href="#SysL2-Sector-Traffic-By-Memory-Aperture">more detailed version of this table</a> can be found below.'

        gen.nodes = [
            gen.Node('System Memory', ['syslts__average_t_sector_aperture_sysmem'], [
                gen.Node('Reads',      ['syslts__average_t_sector_aperture_sysmem_op_read'],  []),
                gen.Node('Writes',     ['syslts__average_t_sector_aperture_sysmem_op_write'], []),
                gen.Node('Atomics',    ['syslts__average_t_sector_aperture_sysmem_op_atom'], []),
                gen.Node('Reductions', ['syslts__average_t_sector_aperture_sysmem_op_red'],  []),
            ]),
            gen.Node('Peer Memory', ['syslts__average_t_sector_aperture_peer'], [
                gen.Node('Reads',      ['syslts__average_t_sector_aperture_peer_op_read'],  []),
                gen.Node('Writes',     ['syslts__average_t_sector_aperture_peer_op_write'], []),
                gen.Node('Atomics',    ['syslts__average_t_sector_aperture_peer_op_atom'], []),
                gen.Node('Reductions', ['syslts__average_t_sector_aperture_peer_op_red'],  []),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class VidL2TrafficBySrcBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow, l2_type_str='Vid')
        gen.name = 'VidL2SectorTrafficBreakdownBySource'
        gen.table_id = 'VidL2-Sector-Traffic-By-Source'
        gen.column_names = [
            ('Source Breakdown', 'From Source'),
            ('Unit Breakdown', 'From Unit'),
            ('Op', 'Op'),
        ]
        gen.workflow += ' This table decomposes VidL2 bandwidth from each source unit, to each destination, per operation. See also: these tables that prioritize <a href="#VidL2-Sector-Traffic-By-Memory-Aperture">destination Memory Aperture</a> and <a href="#VidL2-Sector-Traffic-By-Operation">Operation</a>.'

        gen.nodes = [
            gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc'], [
                gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_tex_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_tex_op_write'], []),
                    gen.Node('Atomics',    ['lts__average_t_sector_srcunit_tex_op_atom'], []),
                    gen.Node('Reductions', ['lts__average_t_sector_srcunit_tex_op_red'],  []),
                ]),
                gen.Node('L1.5 Constant Cache', ['lts__average_t_sector_srcunit_gcc'], [
                    gen.Node('Reads' , ['lts__average_t_sector_srcunit_gcc'], [])
                ]),
                gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_pe_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_pe_op_write'], []),
                ]),
                gen.Node('Raster', ['lts__average_t_sector_srcunit_raster'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_raster_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_raster_op_write'], []),
                ]),
                gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_zrop_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_zrop_op_write'], []),
                ]),
                gen.Node('CROP', ['lts__average_t_sector_srcunit_crop'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_crop_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_crop_op_write'], []),
                ]),
            ]),
            gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp'], [
                gen.Node('all FBP Units', ['lts__average_t_sector_srcnode_fbp'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcnode_fbp_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcnode_fbp_op_write'], []),
                ]),
            ]),
            gen.Node('HUB Units', [], [
                gen.Node('all HUB Units', [], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcnode_hub_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcnode_hub_op_write'], []),
                ]),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class SysL2TrafficBySrcBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow, l2_type_str='Sys')
        gen.name = 'SysL2SectorTrafficBreakdownBySource'
        gen.table_id = 'SysL2-Sector-Traffic-By-Source'
        gen.column_names = [
            ('Source Breakdown', 'From Source'),
            ('Unit Breakdown', 'From Unit'),
            ('Memory Aperture', 'To Memory'),
            ('Op', 'Op'),
        ]
        gen.workflow += ' This table decomposes SysL2 bandwidth from each source unit, to each destination, per operation. See also: these tables that prioritize <a href="#SysL2-Sector-Traffic-By-Memory-Aperture">destination Memory Aperture</a> and <a href="#SysL2-Sector-Traffic-By-Operation">Operation</a>.'

        gen.nodes = [
            gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc'], [
                gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex'], [
                    gen.Node('Peer Memory', ['syslts__average_t_sector_srcunit_tex_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_write'], []),
                        gen.Node('Atomics',    ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_atom'], []),
                        gen.Node('Reductions', ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_red'],  []),
                    ]),
                    gen.Node('System Memory', ['syslts__average_t_sector_srcunit_tex_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_write'], []),
                        gen.Node('Atomics',    ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_atom'], []),
                        gen.Node('Reductions', ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_red'],  []),
                    ]),
                ]),
                gen.Node('L1.5 Constant Cache', ['syslts__average_t_sector_srcunit_gcc'], [
                    gen.Node('Peer Memory', ['syslts__average_t_sector_srcunit_gcc_aperture_peer'], [
                        gen.Node('Reads' , ['syslts__average_t_sector_srcunit_gcc_aperture_peer'], [])
                    ]),
                    gen.Node('System Memory', ['syslts__average_t_sector_srcunit_gcc_aperture_sysmem'], [
                        gen.Node('Reads' , ['syslts__average_t_sector_srcunit_gcc_aperture_sysmem'], [])
                    ]),
                ]),
                gen.Node('Primitive Engine', ['syslts__average_t_sector_srcunit_pe'], [
                    gen.Node('Peer Memory', ['syslts__average_t_sector_srcunit_pe_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_pe_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_pe_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory', ['syslts__average_t_sector_srcunit_pe_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_pe_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_pe_aperture_sysmem_op_write'], []),
                    ]),
                ]),
                gen.Node('Raster', ['lts__average_t_sector_srcunit_raster'], [
                    gen.Node('Peer Memory', ['syslts__average_t_sector_srcunit_raster_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_raster_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_raster_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory', ['syslts__average_t_sector_srcunit_raster_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_raster_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_raster_aperture_sysmem_op_write'], []),
                    ]),
                ]),
                gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop'], [
                    gen.Node('Peer Memory' , ['syslts__average_t_sector_srcunit_zrop_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_zrop_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_zrop_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory' , ['syslts__average_t_sector_srcunit_zrop_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_zrop_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_zrop_aperture_sysmem_op_write'], []),
                    ]),
                ]),
                gen.Node('CROP', ['syslts__average_t_sector_srcunit_crop'], [
                    gen.Node('Peer Memory' , ['syslts__average_t_sector_srcunit_crop_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_crop_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_crop_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory' , ['syslts__average_t_sector_srcunit_crop_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_crop_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_crop_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
            gen.Node('FBP Units', ['syslts__average_t_sector_srcnode_fbp'], [
                gen.Node('all FBP Units', ['syslts__average_t_sector_srcnode_fbp'], [
                    gen.Node('Peer Memory', ['syslts__average_t_sector_srcnode_fbp_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcnode_fbp_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcnode_fbp_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory', ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
            gen.Node('HUB Units', [], [
                gen.Node('all HUB Units', [], [
                    gen.Node('Peer Memory', ['syslts__average_t_sector_srcnode_hub_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcnode_hub_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcnode_hub_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('System Memory', ['syslts__average_t_sector_srcnode_hub_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcnode_hub_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcnode_hub_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class VidL2TrafficByMemoryApertureBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow, l2_type_str='Vid')
        gen.name = 'VidL2SectorTrafficBreakdownByMemoryAperture'
        gen.table_id = 'VidL2-Sector-Traffic-By-Memory-Aperture'
        gen.column_names = [
            ('Source Breakdown', 'From Source'),
            ('Unit Breakdown', 'From Unit'),
            ('Op', 'Op'),
        ]
        gen.workflow += ' This is an extended breakdown of <a href="#VidL2-Sector-Traffic-By-Memory-Aperture-Short">VidL2 Traffic by destination</a>. It decomposes VidL2 bandwidth to each destination, from each source unit, per operation.'

        gen.nodes = [
            gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc'], [
                gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_tex_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_tex_op_write'], []),
                    gen.Node('Atomics',    ['lts__average_t_sector_srcunit_tex_op_atom'], []),
                    gen.Node('Reductions', ['lts__average_t_sector_srcunit_tex_op_red'],  []),
                ]),
                gen.Node('L1.5 Constant Cache', ['lts__average_t_sector_srcunit_gcc'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_gcc'],  []),
                ]),
                gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_pe_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_pe_op_write'], []),
                ]),
                gen.Node('Raster', ['lts__average_t_sector_srcunit_raster'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_raster_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_raster_op_write'], []),
                ]),
                gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_zrop_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_zrop_op_write'], []),
                ]),
                gen.Node('CROP', ['lts__average_t_sector_srcunit_crop'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcunit_crop_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcunit_crop_op_write'], []),
                ]),
            ]),
            gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp'], [
                gen.Node('all FBP Units', ['lts__average_t_sector_srcnode_fbp'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcnode_fbp_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcnode_fbp_op_write'], []),
                ]),
            ]),
            gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub'], [
                gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub'], [
                    gen.Node('Reads',      ['lts__average_t_sector_srcnode_hub_op_read'],  []),
                    gen.Node('Writes',     ['lts__average_t_sector_srcnode_hub_op_write'], []),
                ]),
            ])
        ]

        gen.required_ratios = gen.get_required_ratios()


class SysL2TrafficByMemoryApertureBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow, l2_type_str='Sys')
        gen.name = 'SysL2SectorTrafficBreakdownByMemoryAperture'
        gen.table_id = 'SysL2-Sector-Traffic-By-Memory-Aperture'
        gen.column_names = [
            ('Memory Aperture', 'To Memory'),
            ('Source Breakdown', 'From Source'),
            ('Unit Breakdown', 'From Unit'),
            ('Op', 'Op'),
        ]
        gen.workflow += ' This is an extended breakdown of <a href="#SysL2-Sector-Traffic-By-Memory-Aperture-Short">SysL2 Traffic by destination</a>. It decomposes SysL2 bandwidth to each destination, from each source unit, per operation.'

        gen.nodes = [
            gen.Node('Peer Memory', ['syslts__average_t_sector_aperture_peer'], [
                gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_peer'], [
                    gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_write'], []),
                        gen.Node('Atomics',    ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_atom'], []),
                        gen.Node('Reductions', ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_red'],  []),
                    ]),
                    gen.Node('L1.5 Constant Cache', ['syslts__average_t_sector_srcunit_gcc_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_gcc_aperture_peer'],  []),
                    ]),
                    gen.Node('Primitive Engine', ['syslts__average_t_sector_srcunit_pe_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_pe_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_pe_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('Raster', ['syslts__average_t_sector_srcunit_raster'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_raster_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_raster_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('ZROP', ['syslts__average_t_sector_srcunit_zrop_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_zrop_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_zrop_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('CROP', ['syslts__average_t_sector_srcunit_crop_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_crop_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_crop_aperture_peer_op_write'], []),
                    ]),
                ]),
                gen.Node('FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_peer'], [
                    gen.Node('all FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcnode_fbp_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcnode_fbp_aperture_peer_op_write'], []),
                    ]),
                ]),
                gen.Node('HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_peer'], [
                    gen.Node('all HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_peer'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcnode_hub_aperture_peer_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcnode_hub_aperture_peer_op_write'], []),
                    ]),
                ]),
            ]),
            gen.Node('System Memory', ['syslts__average_t_sector_aperture_sysmem'], [
               gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_sysmem'], [
                    gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_write'], []),
                        gen.Node('Atomics',    ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_atom'], []),
                        gen.Node('Reductions', ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_red'],  []),
                    ]),
                    gen.Node('L1.5 Constant Cache', ['syslts__average_t_sector_srcunit_gcc_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_gcc_aperture_sysmem'],  []),
                    ]),
                    gen.Node('Primitive Engine', ['syslts__average_t_sector_srcunit_pe_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_pe_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_pe_aperture_sysmem_op_write'], []),
                    ]),
                    gen.Node('Raster', ['syslts__average_t_sector_srcunit_raster'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_raster_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_raster_aperture_sysmem_op_write'], []),
                    ]),
                    gen.Node('ZROP', ['syslts__average_t_sector_srcunit_zrop_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_zrop_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_zrop_aperture_sysmem_op_write'], []),
                    ]),
                    gen.Node('CROP', ['syslts__average_t_sector_srcunit_crop_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcunit_crop_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcunit_crop_aperture_sysmem_op_write'], []),
                    ]),
                ]),
                gen.Node('FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem'], [
                    gen.Node('all FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem_op_write'], []),
                    ]),
                ]),
                gen.Node('HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_sysmem'], [
                    gen.Node('all HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_sysmem'], [
                        gen.Node('Reads',      ['syslts__average_t_sector_srcnode_hub_aperture_sysmem_op_read'],  []),
                        gen.Node('Writes',     ['syslts__average_t_sector_srcnode_hub_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class VidL2TrafficByOperationBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow, l2_type_str='Vid')
        gen.name = 'VidL2SectorTrafficBreakdownByOperation'
        gen.table_id = 'VidL2-Sector-Traffic-By-Operation'
        gen.column_names = [
            ('Op', 'Op'),
            ('Source Breakdown', 'From Source'),
            ('Unit Breakdown', 'From Unit'),
        ]
        gen.workflow += ' This table decomposes VidL2 bandwidth per operation, to each destination, from each source unit.'

        gen.nodes = [
            gen.Node('Reads',      ['lts__average_t_sector_op_read'],  [
                gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_op_read'], [
                    gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_op_read'], []),
                    gen.Node('L1.5 Constant Cache', ['lts__average_t_sector_srcunit_gcc'], []),
                    gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_op_read'], []),
                    gen.Node('Raster', ['lts__average_t_sector_srcunit_raster_op_read'], []),
                    gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_op_read'], []),
                    gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_op_read'], []),
                ]),
                gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_op_read'], [
                    gen.Node('all FBP Units', ['lts__average_t_sector_srcnode_fbp_op_read'], []),
                ]),
                gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_op_read'], [
                    gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_op_read'], []),
                ]),
            ]),
            gen.Node('Writes',     ['lts__average_t_sector_op_write'], [
                gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_op_write'], [
                    gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_op_write'], []),
                    gen.Node('Primitive Engine', ['lts__average_t_sector_srcunit_pe_op_write'], []),
                    gen.Node('Raster', ['lts__average_t_sector_srcunit_raster_op_write'], []),
                    gen.Node('ZROP', ['lts__average_t_sector_srcunit_zrop_op_write'], []),
                    gen.Node('CROP', ['lts__average_t_sector_srcunit_crop_op_write'], []),
                ]),
                gen.Node('FBP Units', ['lts__average_t_sector_srcnode_fbp_op_write'], [
                    gen.Node('all FBP Units', ['lts__average_t_sector_srcnode_fbp_op_write'], []),
                ]),
                gen.Node('HUB Units', ['lts__average_t_sector_srcnode_hub_op_write'], [
                    gen.Node('all HUB Units', ['lts__average_t_sector_srcnode_hub_op_write'], []),
                ]),
            ]),
            gen.Node('Atomics',    ['lts__average_t_sector_op_atom'], [
                gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_op_atom'], [
                    gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_op_atom'], []),
                ]),
            ]),
            gen.Node('Reductions', ['lts__average_t_sector_op_red'], [
                gen.Node('GPC Units', ['lts__average_t_sector_srcnode_gpc_op_red'], [
                    gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['lts__average_t_sector_srcunit_tex_op_red'], []),
                ]),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()


class SysL2TrafficByOperationBreakdownGenerator(tables_common.L2TrafficBreakdownGenerator):
    def __init__(gen, show_generic_workflow):
        super().__init__(show_generic_workflow=show_generic_workflow, l2_type_str='Sys')
        gen.name = 'SysL2SectorTrafficBreakdownByOperation'
        gen.table_id = 'SysL2-Sector-Traffic-By-Operation'
        gen.column_names = [
            ('Op', 'Op'),
            ('Memory Aperture', 'To Memory'),
            ('Source Breakdown', 'From Source'),
            ('Unit Breakdown', 'From Unit'),
        ]
        gen.workflow += ' This table decomposes SysL2 bandwidth per operation, to each destination, from each source unit.'

        gen.nodes = [
            gen.Node('Reads',      ['syslts__average_t_sector_op_read'],  [
                gen.Node('Peer Memory', ['syslts__average_t_sector_aperture_peer_op_read'], [
                    gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_peer_op_read'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_read'], []),
                        gen.Node('L1.5 Constant Cache', ['syslts__average_t_sector_srcunit_gcc_aperture_peer'], []), # gcc only has reads
                        gen.Node('Primitive Engine', ['syslts__average_t_sector_srcunit_pe_aperture_peer_op_read'], []),
                        gen.Node('Raster', ['syslts__average_t_sector_srcunit_raster_aperture_peer_op_read'], []),
                        gen.Node('ZROP', ['syslts__average_t_sector_srcunit_zrop_aperture_peer_op_read'], []),
                        gen.Node('CROP', ['syslts__average_t_sector_srcunit_crop_aperture_peer_op_read'], []),
                    ]),
                    gen.Node('FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_peer_op_read'], [
                        gen.Node('all FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_peer_op_read'], []),
                    ]),
                    gen.Node('HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_peer_op_read'], [
                        gen.Node('all HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_peer_op_read'], []),
                    ]),
                ]),
                gen.Node('System Memory', ['syslts__average_t_sector_aperture_sysmem_op_read'], [
                    gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_sysmem_op_read'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_read'], []),
                        gen.Node('L1.5 Constant Cache', ['syslts__average_t_sector_srcunit_gcc_aperture_sysmem'], []),
                        gen.Node('Primitive Engine', ['syslts__average_t_sector_srcunit_pe_aperture_sysmem_op_read'], []),
                        gen.Node('Raster', ['syslts__average_t_sector_srcunit_raster_aperture_sysmem_op_read'], []),
                        gen.Node('ZROP', ['syslts__average_t_sector_srcunit_zrop_aperture_sysmem_op_read'], []),
                        gen.Node('CROP', ['syslts__average_t_sector_srcunit_crop_aperture_sysmem_op_read'], []),
                    ]),
                    gen.Node('FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem_op_read'], [
                        gen.Node('all FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem_op_read'], []),
                    ]),
                    gen.Node('HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_sysmem_op_read'], [
                        gen.Node('all HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_sysmem_op_read'], []),
                    ]),
                ]),
            ]),
            gen.Node('Writes',     ['syslts__average_t_sector_op_write'], [
                gen.Node('Peer Memory', ['syslts__average_t_sector_aperture_peer_op_write'], [
                    gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_peer_op_write'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_write'], []),
                        gen.Node('Primitive Engine', ['syslts__average_t_sector_srcunit_pe_aperture_peer_op_write'], []),
                        gen.Node('Raster', ['syslts__average_t_sector_srcunit_raster_aperture_peer_op_write'], []),
                        gen.Node('ZROP', ['syslts__average_t_sector_srcunit_zrop_aperture_peer_op_write'], []),
                        gen.Node('CROP', ['syslts__average_t_sector_srcunit_crop_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_peer_op_write'], [
                        gen.Node('all FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_peer_op_write'], []),
                    ]),
                    gen.Node('HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_peer_op_write'], [
                        gen.Node('all HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_peer_op_write'], []),
                    ]),
                ]),
                gen.Node('System Memory', ['syslts__average_t_sector_aperture_sysmem_op_write'], [
                    gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_sysmem_op_write'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_write'], []),
                        gen.Node('Primitive Engine', ['syslts__average_t_sector_srcunit_pe_aperture_sysmem_op_write'], []),
                        gen.Node('Raster', ['syslts__average_t_sector_srcunit_raster_aperture_sysmem_op_write'], []),
                        gen.Node('ZROP', ['syslts__average_t_sector_srcunit_zrop_aperture_sysmem_op_write'], []),
                        gen.Node('CROP', ['syslts__average_t_sector_srcunit_crop_aperture_sysmem_op_write'], []),
                    ]),
                    gen.Node('FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem_op_write'], [
                        gen.Node('all FBP Units', ['syslts__average_t_sector_srcnode_fbp_aperture_sysmem_op_write'], []),
                    ]),
                    gen.Node('HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_sysmem_op_write'], [
                        gen.Node('all HUB Units', ['syslts__average_t_sector_srcnode_hub_aperture_sysmem_op_write'], []),
                    ]),
                ]),
            ]),
            gen.Node('Atomics',    ['syslts__average_t_sector_op_atom'], [
                gen.Node('Peer Memory', ['syslts__average_t_sector_aperture_peer_op_atom'], [
                    gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_peer_op_atom'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_atom'], []),
                    ]),
                ]),
                gen.Node('System Memory', ['syslts__average_t_sector_aperture_sysmem_op_atom'], [
                    gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_sysmem_op_atom'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_atom'], []),
                    ]),
                ]),
            ]),
            gen.Node('Reductions', ['syslts__average_t_sector_op_red'], [
                gen.Node('Peer Memory', ['syslts__average_t_sector_aperture_peer_op_red'], [
                    gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_peer_op_red'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_peer_op_red'], []),
                    ]),
                ]),
                gen.Node('System Memory', ['syslts__average_t_sector_aperture_sysmem_op_red'], [
                    gen.Node('GPC Units', ['syslts__average_t_sector_srcnode_gpc_aperture_sysmem_op_red'], [
                        gen.Node('<a href="#L1TEX-Sector-Traffic">L1TEX Cache</a>', ['syslts__average_t_sector_srcunit_tex_aperture_sysmem_op_red'], []),
                    ]),
                ]),
            ]),
        ]

        gen.required_ratios = gen.get_required_ratios()

class RasterDataflowGenerator(tables_common.RasterDataflowGenerator):
    def __init__(gen):
        zrop_pixels_input = 'prop__prop2zrop_pixels_realtime'
        crop_pixels_input = 'prop__prop2crop_pixels_realtime'
        super().__init__(zrop_pixels_input, crop_pixels_input)
        gen.rows = [
            #       stage                           , pixelsInput                                   , pixelsKilled                                            , pixelsOutput                                            , samplesInput                          , samplesKilled                                   , samplesOutput
            gen.Row('ZCULL'                         , "raster__zcull_input_samples"                 , "raster__zcull_input_samples_op_rejected"               , "raster__zcull_input_samples_op_accepted"               , "NotApplicable"                       , "NotApplicable"                                 , "NotApplicable"               ),
            gen.Row('PROP Input'                    , "prop__input_pixels_type_3d_realtime"         , "NotApplicable"                                         , "NotApplicable"                                         , "NotAvailable"                        , "NotApplicable"                                 , "NotApplicable"               ),
            gen.Row('PROP EarlyZ'                   , "prop__prop2zrop_pixels_mode_earlyz_realtime" , "prop__prop2zrop_pixels_mode_earlyz_op_killed_realtime" , "prop__prop2zrop_pixels_mode_earlyz_op_passed_realtime" , "prop__prop2zrop_samples_mode_earlyz" , "prop__prop2zrop_samples_mode_earlyz_op_killed" , "prop__prop2zrop_samples_mode_earlyz_op_passed" ),
            gen.Row('Pixel Shader(EarlyZ + LateZ)'  , "sm__threads_launched_shader_ps_killmask_off" , "NotAvailable"                                          , "NotAvailable"                                          , "NotApplicable"                       , "NotApplicable"                                 , "NotApplicable"               ),
            gen.Row('PROP LateZ'                    , "prop__prop2zrop_pixels_mode_latez_realtime"  , "prop__prop2zrop_pixels_mode_latez_op_killed_realtime"  , "prop__prop2zrop_pixels_mode_latez_op_passed_realtime"  , "prop__prop2zrop_samples_mode_latez"  , "prop__prop2zrop_samples_mode_latez_op_killed"  , "prop__prop2zrop_samples_mode_latez_op_passed"  ),
            gen.Row('ZROP'                          , zrop_pixels_input                             , "NotApplicable"                                         , "NotApplicable"                                         , "NotAvailable"                        , "NotAvailable"                                  , "NotAvailable"                ),
            gen.Row('PROP Color'                    , "NotAvailable"                                , "NotAvailable"                                          , "NotAvailable"                                          , "NotAvailable"                        , "NotAvailable"                                  , "NotAvailable"                ),
            gen.Row('CROP'                          , crop_pixels_input                             , "NotAvailable"                                          , "NotAvailable"                                          , "NotAvailable"                        , "NotAvailable"                                  , "NotAvailable"                ),
        ]
        gen.required_counters = [
            'prop__prop2zrop_samples_mode_earlyz',
            'prop__prop2zrop_samples_mode_earlyz_op_killed',
            'prop__prop2zrop_samples_mode_earlyz_op_passed',
            'prop__prop2zrop_samples_mode_latez',
            'prop__prop2zrop_samples_mode_latez_op_killed',
            'prop__prop2zrop_samples_mode_latez_op_passed',
            'prop__prop2zrop_pixels_mode_earlyz_realtime',
            'prop__prop2zrop_pixels_mode_earlyz_op_killed_realtime',
            'prop__prop2zrop_pixels_mode_earlyz_op_passed_realtime',
            'prop__prop2zrop_pixels_mode_latez_realtime',
            'prop__prop2zrop_pixels_mode_latez_op_killed_realtime',
            'prop__prop2zrop_pixels_mode_latez_op_passed_realtime',
            'prop__input_pixels_type_3d_realtime',
            'raster__zcull_input_samples',
            'raster__zcull_input_samples_op_accepted',
            'raster__zcull_input_samples_op_rejected',
            'sm__threads_launched_shader_ps_killmask_off',
            'prop__prop2zrop_pixels_realtime',
            'prop__prop2crop_pixels_realtime',
        ]

class AdditionalMetricsGenerator(tables_common.AdditionalMetricsGenerator):
    def __init__(gen):
        super().__init__()
        gen.required_counters = [
            'idc__requests',
            'idc__requests_lookup_hit',
            'idc__requests_lookup_miss',
            'l1tex__data_pipe_lsu_wavefronts_mem_lgds',
            'l1tex__data_pipe_lsu_wavefronts_mem_shared',
            'l1tex__data_pipe_lsu_wavefronts_mem_surface',
            'l1tex__data_pipe_tex_wavefronts_mem_surface',
            'l1tex__data_pipe_tex_wavefronts_mem_texture',
            'l1tex__lsu_writeback_active',
            'l1tex__lsuin_requests',
            'l1tex__tex_writeback_active',
            'l1tex__texin_cycles_stalled_on_tsl1_miss',
            'l1tex__texin_requests',
            'l1tex__texin_sm2tex_req_cycles_active',
            'sm__mio_pq_read_cycles_active',
            'sm__mio_pq_write_cycles_active',
            'sm__mio2rf_writeback_active',
            'sm__ps_quads_launched',
            'smsp__thread_inst_executed',
            'smsp__thread_inst_executed_pred_off',
            'smsp__thread_inst_executed_pred_on',
            'smsp__warps_eligible',
        ]


class WarpIssueStallsGenerator(tables_common.WarpIssueStallsGenerator):
    def __init__(gen):
        super().__init__()
        gen.rows = [
            gen.Row('All; sum of all other reasons (waiting + issuing)'                                             , 'Total'                 , 'smsp__warps_active'                          ),
            gen.Row('Waiting for a synchronization barrier'                                                         , 'Barrier'               , 'smsp__warps_issue_stalled_barrier'           ),
            gen.Row('Waiting for dynamic branch target computation and warp PC update'                              , 'Branch Resolving'      , 'smsp__warps_issue_stalled_branch_resolving'  ),
            gen.Row('Waiting for a busy pipeline at the dispatch stage; wasted scheduler cycle'                     , 'Dispatch Stall'        , 'smsp__warps_issue_stalled_dispatch_stall'    ),
            gen.Row('Waiting for memory instructions or pixout to complete after warp EXIT'                         , 'Drain after Exit'      , 'smsp__warps_issue_stalled_drain'             ),
            gen.Row('Waiting for free space in the LG Input FIFO: local/global instruction issue to LSU'            , 'LG Throttle'           , 'smsp__warps_issue_stalled_lg_throttle'       ),
            gen.Row('Waiting on variable latency dependency: LSU (local/global), TEX (texture, surface)'            , 'Long Scoreboard'       , 'smsp__warps_issue_stalled_long_scoreboard'   ),
            gen.Row('Waiting for a math pipe to become available'                                                   , 'Math Pipe Throttle'    , 'smsp__warps_issue_stalled_math_pipe_throttle'),
            gen.Row('Waiting for a memory barrier'                                                                  , 'Memory Barrier'        , 'smsp__warps_issue_stalled_membar'            ),
            gen.Row('Waiting for free space in the MIO Input FIFO: ADU, CBU, FP64, LSU (not local/global), IPA'     , 'MIO Throttle'          , 'smsp__warps_issue_stalled_mio_throttle'      ),
            gen.Row('Waiting for a miscellaneous reason (should be rare)'                                           , 'Misc'                  , 'smsp__warps_issue_stalled_misc'              ),
            gen.Row('Waiting for instruction fetch, or instruction cache miss'                                      , 'No Instruction'        , 'smsp__warps_issue_stalled_no_instruction'    ),
            gen.Row('Waiting for a scheduler cycle, from an otherwise ready-to-issue warp'                          , 'Not Selected'          , 'smsp__warps_issue_stalled_not_selected'      ),
            gen.Row('Issuing an instruction'                                                                        , 'Selected'              , 'smsp__warps_issue_stalled_selected'          ),
            gen.Row('Waiting on variable latency dependency: XU, FP64, LSU (not local/global), ADU, CBU, LDC/IDC'   , 'Short Scoreboard'      , 'smsp__warps_issue_stalled_short_scoreboard'  ),
            gen.Row('Waiting for a nanosleep timer to expire'                                                       , 'Sleeping'              , 'smsp__warps_issue_stalled_sleeping'          ),
            gen.Row('Waiting for free space in the TEX Input FIFO: TEX pipe texture or surface instructions'        , 'TEX Throttle'          , 'smsp__warps_issue_stalled_tex_throttle'      ),
            gen.Row('Waiting on a fixed latency dependency'                                                         , 'Wait'                  , 'smsp__warps_issue_stalled_wait'              ),
        ]
        gen.required_counters = [
            'smsp__inst_executed',
            'smsp__warps_launched',
            'smsp__warps_active',
            'smsp__warps_issue_stalled_barrier',
            'smsp__warps_issue_stalled_branch_resolving',
            'smsp__warps_issue_stalled_dispatch_stall',
            'smsp__warps_issue_stalled_drain',
            'smsp__warps_issue_stalled_lg_throttle',
            'smsp__warps_issue_stalled_long_scoreboard',
            'smsp__warps_issue_stalled_math_pipe_throttle',
            'smsp__warps_issue_stalled_membar',
            'smsp__warps_issue_stalled_mio_throttle',
            'smsp__warps_issue_stalled_misc',
            'smsp__warps_issue_stalled_no_instruction',
            'smsp__warps_issue_stalled_not_selected',
            'smsp__warps_issue_stalled_selected',
            'smsp__warps_issue_stalled_short_scoreboard',
            'smsp__warps_issue_stalled_sleeping',
            'smsp__warps_issue_stalled_tex_throttle',
            'smsp__warps_issue_stalled_wait',
        ]