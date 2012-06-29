## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

import os
import Options
import os.path
import ns3waf
import sys


def options(opt):
    opt.tool_options('compiler_cc') 
    ns3waf.options(opt)
    opt.add_option('--enable-kernel-stack',
                   help=('Path to the prefix where the kernel wrapper headers are installed'),
                   default=None,
                   dest='kernel_stack', type="string")

def configure(conf):
    ns3waf.check_modules(conf, ['dce'], mandatory = True)
    ns3waf.check_modules(conf, ['dce-quagga'], mandatory = True)
    ns3waf.check_modules(conf, ['core', 'network', 'internet'], mandatory = True)
    ns3waf.check_modules(conf, ['point-to-point', 'tap-bridge', 'netanim'], mandatory = False)
    ns3waf.check_modules(conf, ['wifi', 'point-to-point', 'csma', 'mobility'], mandatory = False)
    ns3waf.check_modules(conf, ['point-to-point-layout'], mandatory = False)
    ns3waf.check_modules(conf, ['topology-read', 'applications', 'visualizer'], mandatory = False)
    conf.check_tool('compiler_cc')
    conf.check(header_name='stdint.h', define_name='HAVE_STDINT_H', mandatory=False)
    conf.check(header_name='inttypes.h', define_name='HAVE_INTTYPES_H', mandatory=False)
    conf.check(header_name='sys/inttypes.h', define_name='HAVE_SYS_INT_TYPES_H', mandatory=False)
    conf.check(header_name='sys/types.h', define_name='HAVE_SYS_TYPES_H', mandatory=False)
    conf.check(header_name='sys/stat.h', define_name='HAVE_SYS_STAT_H', mandatory=False)
    conf.check(header_name='dirent.h', define_name='HAVE_DIRENT_H', mandatory=False)

    conf.env.append_value('LINKFLAGS', '-pthread')
    conf.env.append_value('LINKFLAGS', '-Wl,--dynamic-linker=' +
                             os.path.abspath ('../build/lib/ldso'))
    conf.check (lib='dl', mandatory = True)
    ns3waf.print_feature_summary(conf)


def dce_kw(**kw):
    d = dict(**kw)
    if os.uname()[4] == 'x86_64':
        mcmodel = ['-mcmodel=large']
    else:
        mcmodel = []
    nofortify = ['-U_FORTIFY_SOURCE']
    #debug_dl = ['-Wl,--dynamic-linker=/usr/lib/debug/ld-linux-x86-64.so.2']
    debug_dl = []
    d['cxxflags'] = d.get('cxxflags', []) + ['-fpie'] + mcmodel + nofortify
    d['cflags'] = d.get('cflags', []) + ['-fpie'] + mcmodel + nofortify
    d['linkflags'] = d.get('linkflags', []) + ['-pie'] + debug_dl
    return d

def build_dce_tests(module, kern):
    module.add_runner_test(needed=['core', 'dce-quagga', 'internet', 'csma', 'dce-umip'],
                           source=['test/dce-umip-test.cc'])

def build_dce_examples(module):
    dce_examples = [
                   ]
    for name,lib in dce_examples:
        module.add_example(**dce_kw(target = 'bin/' + name, 
                                    source = ['example/' + name + '.cc'],
                                    lib = lib))

def build_dce_kernel_examples(module):
    module.add_example(needed = ['core', 'internet', 'dce', 'csma', 'mobility', 'wifi', 'dce-quagga', 'dce-umip'],
                       target='bin/dce-umip-cmip6',
                       source=['example/dce-umip-cmip6.cc'])

    module.add_example(needed = ['core', 'internet', 'dce', 'csma', 'mobility', 'wifi', 'dce-quagga', 'dce-umip'],
                       target='bin/dce-umip-nemo',
                       source=['example/dce-umip-nemo.cc'])

    module.add_example(needed = ['core', 'internet', 'dce', 'csma', 'mobility', 'wifi', 'dce-quagga', 'dce-umip'],
                       target='bin/dce-umip-dsmip6',
                       source=['example/dce-umip-dsmip6.cc'])

    module.add_example(needed = ['core', 'internet', 'dce', 'csma', 'mobility', 'wifi', 'dce-quagga', 'dce-umip'],
                       target='bin/dce-umip-pmip6',
                       source=['example/dce-umip-pmip6.cc'])




def build(bld):
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

    build_dce_tests(module, bld.env['KERNEL_STACK'])
    build_dce_examples(module)
    build_dce_kernel_examples(module)
