## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

import os
import os.path
import ns3waf
import sys
from waflib import Utils, Scripting, Configure, Build, Options, TaskGen, Context, Task, Logs, Errors

# local modules


def options(opt):
    opt.tool_options('compiler_cc') 
    ns3waf.options(opt)

def configure(conf):
    if 'KERNEL_STACK' not in conf.env:
        return

    ns3waf.check_modules(conf, ['core', 'network', 'internet'], mandatory = True)
    ns3waf.check_modules(conf, ['point-to-point', 'tap-bridge', 'netanim'], mandatory = False)
    ns3waf.check_modules(conf, ['wifi', 'point-to-point', 'csma', 'mobility'], mandatory = False)
    ns3waf.check_modules(conf, ['point-to-point-layout'], mandatory = False)
    ns3waf.check_modules(conf, ['topology-read', 'applications', 'visualizer'], mandatory = False)

    conf.env.append_value('LINKFLAGS', '-pthread')
    conf.check (lib='dl', mandatory = True)
    conf.env['ENABLE_PYTHON_BINDINGS'] = True
    conf.env['NS3_ENABLED_MODULES'] = []
    ns3waf.print_feature_summary(conf)


def build_dce_tests(module, bld):
    module.add_runner_test(needed=['core', 'dce-quagga', 'internet', 'csma', 'dce-umip', 'mobility', 'wifi'],
                           source=['test/dce-umip-test.cc'])
def build_dce_examples(module):
    dce_examples = [
                   ]
    for name,lib in dce_examples:
        module.add_example(**dce_kw(target = 'bin/' + name, 
                                    source = ['example/' + name + '.cc'],
                                    lib = lib))

def build_dce_kernel_examples(module):
    module.add_example(needed = ['core', 'internet', 'csma', 'mobility', 'wifi', 'dce-umip'],
                       target='bin/dce-umip-cmip6',
                       source=['example/dce-umip-cmip6.cc'])

    module.add_example(needed = ['core', 'internet', 'csma', 'mobility', 'wifi', 'dce-umip'],
                       target='bin/dce-umip-nemo',
                       source=['example/dce-umip-nemo.cc'])

#    module.add_example(needed = ['core', 'internet', 'csma', 'mobility', 'wifi', 'dce-umip', 'applications'],
#                       target='bin/dce-umip-dsmip6',
#                       source=['example/dce-umip-dsmip6.cc'])
#
#    module.add_example(needed = ['core', 'internet', 'csma', 'mobility', 'wifi', 'dce-umip', 'applications'],
#                       target='bin/dce-umip-pmip6',
#                       source=['example/dce-umip-pmip6.cc'])

def build(bld):
    if 'KERNEL_STACK' not in bld.env:
        return

    module_source = [
        'helper/mip6d-helper.cc',
        ]
    module_headers = [
        'helper/mip6d-helper.h',
        ]
    module_source = module_source
    module_headers = module_headers
    uselib = ns3waf.modules_uselib(bld, ['core', 'network', 'internet', 'netlink', 'dce', 'dce-quagga'])
    module = ns3waf.create_module(bld, name='dce-umip',
                                  source=module_source,
                                  headers=module_headers,
                                  use=uselib,
                                  lib=['dl'])

    build_dce_tests(module, bld)
    build_dce_examples(module)
    build_dce_kernel_examples(module)
