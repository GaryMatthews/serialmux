#!/usr/bin/scons
#
# Top-level build file for the Dust Networks Serial Multiplexer
#

import sys
import os
import re
import SCons
from SCons.Script import *
import shutil

import platform
# TODO: if platform.uname()[0] starts with 'CYGWIN':
# print instructions for running under Windows
# OR support cygwin by running MSBuild using cmd.exe /C 

# the shared/build directory is configured as a svn external, so it's always a
# subdirectory
sys.path += ['shared/build']

# Add some command line variables to SCons to support different build types.
command_line_options = Variables()
command_line_options.AddVariables(
    ('debug', 'Set to 1 to build with debugging enabled', 1),
    ('python', 'Path to the python interpreter', 'python'),
    ('boost_base', 'Path to the root of the Boost install', ''),
    ('boost_lib_suffix', 'Library name suffix for Boost libraries', ''),
    ('cxx', 'Name of the C++ compiler', 'g++'),
)

# Give some usage hints for this project
Help("""
Build Serial Mux using the Microsoft Visual Studio project:

 scons mux

Build and release the Serial Mux:

 scons release

Unit tests are built with the Boost Unit Test harness. 

Run all unit tests with XML output:

 \Python\Scripts\scons run-tests

Miscellaneous targets:

  incr-version: increment the build number

""")


baseEnv = Environment(options = command_line_options)

# Ensure that no default targets exist, so you have to specify a target.
# Give some help about what targets are available.
def default(target, source, env): print SCons.Script.help_text
Default(baseEnv.Command('default', None, default))

# Find subversion
baseEnv['SVN_BINARY'] = 'svn.exe'  # by default, trust that the path is good

# on Windows, make sure that the native subversion is used
SVN_PATH = [ os.path.join('\\','Program Files','CollabNet','Subversion Client'),
             os.path.join('\\','Program Files','CollabNet Subversion'),
             ]

for d in SVN_PATH:
    svn = os.path.join(d, 'svn.exe')
    if os.path.isfile(svn):
        baseEnv['SVN_BINARY'] = svn

# Paths

baseEnv['SHARED_DIR'] = '.'

baseEnv['VERSION_FILE'] = os.path.join('serial_mux', 'Version.h')
baseEnv['BUILD_FILE'] = os.path.join('serial_mux', 'Build.h')

baseEnv.Append(CPPPATH = ['$BOOST_BASE',
                      baseEnv['SHARED_DIR'] + '/shared/include',
                      baseEnv['SHARED_DIR'] + '/shared/include/cxxtest',
                      'serial_mux',
                      'ext-tools/LogUtilities'])
baseEnv.Append(LIBPATH = ['$BOOST_BASE' + '/stage/lib'])


# Build Tool Paths

def getLinuxEnv(baseEnv):
    env = baseEnv.Clone()
    env['ENGR_PUBLISH_DIR'] = os.path.join('/mnt', 'engineering', 'Software',
                                           'Builds', 'SerialMux')
    if env['boost_base']:
        env['BOOST_BASE'] = env['boost_base']
    else:
        env['BOOST_BASE'] = os.path.join('/mnt', 'sdev', 'tools', 'boost', 'boost_1_46_1')

    if env['cxx']:
        env['CXX'] = env['cxx']

    env.Append(CPPPATH = ['ext-tools'],
               LINKFLAGS = ['-Wl,-Bstatic'], # link Boost libraries statically
               LINKCOM = ' -Wl,-Bdynamic -lpthread', # some libraries must be dynamic
               )

    return env

def getOSXEnv(baseEnv):
    env = baseEnv.Clone()
    env['ENGR_PUBLISH_DIR'] = os.path.join('/Users', 'Shared', 'Software',
                                           'Builds', 'SerialMux')
    env['BOOST_BASE'] = os.path.join('/Users', 'Shared', 'boost', 'boost_1_46_1')
    env.Append(CPPPATH = ['ext-tools'])
    
    return env

def getWinEnv(baseEnv):
    env = baseEnv.Clone()
    env['ENGR_PUBLISH_DIR'] = os.path.join('\\\\filer01', 'Engineering', 'Software',
                                           'Builds', 'SerialMux')
    
    # Build configuration
    env['DOTNET_VERSION'] = 'v4.0.30319'
    env['NUNIT_VERSION'] = '2.5.7'
    # TODO: support command line config of Build type
    env['BUILD_CONFIGURATION'] = 'Release' 

    env['MSBuild'] = os.path.join('\\', 'Windows', 'Microsoft.NET', 'Framework', 
                                  env['DOTNET_VERSION'], 'MSBuild.exe')
    env['NUnit'] = os.path.join('\\', 'Program Files', 'NUnit ' + env['NUNIT_VERSION'], 
                                'bin', 'net-2.0', 'nunit-console.exe')
    
    return env


if os.name in ['nt']:
    baseEnv['platform'] = 'win32'
    env = getWinEnv(baseEnv)

elif platform.system() in ['Linux']:
    baseEnv['platform'] = 'linux'
    env = getLinuxEnv(baseEnv)

elif platform.system() in ['Darwin']:
    baseEnv['platform'] = 'osx'
    env = getOSXEnv(baseEnv)

else:
    env = baseEnv.Clone()
    print 'Unknown platform:', platform.system()


# Serial Mux sources

serial_mux_sources = [ 'serial_mux/serial_mux.cpp',
                       'serial_mux/BasePicard.cpp',
                       'serial_mux/BoostClient.cpp',
                       'serial_mux/BoostClientListener.cpp',
                       'serial_mux/BoostClientManager.cpp',
                       'serial_mux/Common.cpp',
                       'serial_mux/HDLC.cpp',
                       'serial_mux/MuxMessageParser.cpp',
                       'serial_mux/PicardBoost.cpp',
                       'serial_mux/SerialMuxOptions.cpp',
                       'serial_mux/Subscriber.cpp',
                       'serial_mux/Version.cpp',
                       'ext-tools/LogUtilities/BoostLog.cpp',
                       ]


# Serial Mux targets

if env['platform'] in ['win32']: 
    mux_exe = os.path.join(env['BUILD_CONFIGURATION'], 'serial_mux.exe')
    sln = os.path.join('serial_mux.sln')
    serial_mux = env.Command(mux_exe, sln,
                             Action('"%s" /target:serial_mux /property:Configuration=%s' %
                                    (env['MSBuild'], env['BUILD_CONFIGURATION'])))
    Alias('mux', serial_mux)

    # NUnit tests
    unittests_dll = os.path.join(env['BUILD_CONFIGURATION'], 'unit_tests.dll')
    unittests = env.Command(unittests_dll, sln,
                            Action('"%s" /target:unit_tests /property:Configuration=%s' %
                                   (env['MSBuild'], env['BUILD_CONFIGURATION'])))
    
    runtests = env.Command('TestResults.xml', unittests,
                           Action('"%s" $SOURCES' % (env['NUnit'])))
    Alias('run-tests', runtests)
    
elif env['platform'] in ['osx']:
    mux_binary = 'serial_mux_%s' % env['platform']
    serial_mux = env.Program(mux_binary, serial_mux_sources,
                             LIBS = ['boost_date_time',
                                     'boost_program_options',
                                     'boost_filesystem',
                                     'boost_system',
                                     'boost_thread',
                                     'pthread'])

elif env['platform'] in ['linux']:
    mux_binary = 'serial_mux_%s' % env['platform']
    serial_mux = env.Program(mux_binary, serial_mux_sources,
                             LIBS = ['boost_date_time${boost_lib_suffix}',
                                     'boost_program_options${boost_lib_suffix}',
                                     'boost_filesystem${boost_lib_suffix}',
                                     'boost_system${boost_lib_suffix}',
                                     'boost_thread${boost_lib_suffix}',
                                     ])

Alias('mux', serial_mux)


# ----------------------------------------------------------------------
# Release actions

from VersionStandalone import CVersion
from SvnStandalone import SvnStandalone

env['Version'] = CVersion(env['VERSION_FILE'], env['BUILD_FILE'])

env['Svn'] = SvnStandalone(version_object = env['Version'])


get_version = env.Command('always.version', [ env['VERSION_FILE'], env['BUILD_FILE'] ],
                          'echo %s' % env['Version'].get_version_str())
Alias('get-version', get_version)

incr_version = env.Command('always.incr_build', env['BUILD_FILE'],
                           Action(env['Version'].increment_version_action))
Alias('incr-version', incr_version)

tag_incr = env.Command('always.tag_and_incr', [  ],
                       env['Svn'].tag_and_increment_action,
                       label='SerialMux_%s' % env['Version'].get_version_str(),
                       project_name='tools/SerialMux')
AlwaysBuild(tag_incr)
Alias('tag_incr', tag_incr)


def distribute_action(target, source, env):
    '''
    Publish the files to the release directory
    '''
    dry_run = env.has_key('dry_run') and bool(env['dry_run'])

    if not os.path.isdir(env['DEST_DIR']) and not dry_run:
        os.makedirs(env['DEST_DIR'])

    for src in source:
        print "Publishing %s to %s" % (src.path, env['DEST_DIR'])
        #src_tail, src_ext = os.path.splitext(os.path.split(src.path)[1])
        #dest_file = '%s_%s%s' % (src_tail, env['Version'].get_version_str(), src_ext)
        dest_file = os.path.split(src.path)[1]
        if not dry_run:
            shutil.copyfile(src.path, os.path.join(env['DEST_DIR'], dest_file))
        else:
            print 'DRY RUN: copying %s to %s' % (src.path, os.path.join(env['DEST_DIR'], dest_file))
    return 0

versioned_dir = os.path.join(env['ENGR_PUBLISH_DIR'],
                             'SerialMux_%s' % env['Version'].get_version_str())

# Ug. scons doesn't handle Windows server paths well. It assumes they are on
# the current drive (even with a \\ prefix). So we pass the destination
# through the environment. 
env['DEST_DIR'] = versioned_dir
publish_exe = env.Command('always.publish', serial_mux,
                          Action(distribute_action))

Alias('publish', publish_exe)

Alias('release', [publish_exe, tag_incr])

