# SONIC ROBO BLAST 2
# -----------------------------------------------------------------------------
# Copyright (C) 1993-1996 by id Software, Inc.
# Copyright (C) 1998-2000 by DooM Legacy Team.
# Copyright (C) 1999-2020 by Sonic Team Junior.

# This program is free software distributed under the
# terms of the GNU General Public License, version 2.
# See the 'LICENSE' file for more details.
# -----------------------------------------------------------------------------
# / \file  emscripten-package.py
# / \brief Prepares a deployable Emscripten package for web.

import sys
import os
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(script_dir, '..', 'tools'))

from fwad import convert_fwad # local package -- {repo_root}/tools/fwad
from md5 import dir_md5  # local package -- {repo_root}/emscripten/md5
import time
import base64
from shutil import copy
from zipfile import ZipFile

defaults = {
    'build_dir': f'{script_dir}/../bin/Emscripten/Release',
    'landing_dir': f'{script_dir}/landing',
    'data_dir': f'{script_dir}/data',
    'url': f'https://srb2web.surge.sh'
}

def populate_shell_template(shell_version, gtag, package_versions, default_package_version, landing_dir=defaults['landing_dir'],
                            url=defaults['url'], maintainer=None, maintainer_url=None):
    shell_data = ''
    landing_fn = os.path.join(landing_dir, 'index.html')
    copy(os.path.join(script_dir, 'srb2.html'), landing_fn)

    package_version_list = []
    for version in package_versions:
        selected = ''
        if version == default_package_version and not len(selected):
            selected = ' selected'
        package_version_list.append(f'<option value="{version}"{selected}>{version}</option>')

    if maintainer is not None:
        if maintainer_url is not None:
            maintainer = f'<a href="{maintainer_url}" target="_blank">{maintainer}</a>'
        maintainer_string = f'This web service is maintained by {maintainer}. It is not an official project of Sonic Team Junior.'
    else:
        maintainer_string = 'This web service is not an official project of Sonic Team Junior.'

    with open(landing_fn, 'r') as f:
        shell_data = f.read()

    shell_data = shell_data.replace('{{{ URL }}}', url)
    shell_data = shell_data.replace('{{{ PACKAGE_VERSION }}}', default_package_version)
    shell_data = shell_data.replace('{{{ SHELL_VERSION }}}', shell_version)
    shell_data = shell_data.replace('<!-- {{{ GTAG }}} -->', gtag if gtag else '')
    shell_data = shell_data.replace('<!-- {{{ PACKAGE_VERSION_LIST }}} -->', '\n'.join(package_version_list))
    shell_data = shell_data.replace('{{{ MAINTAINER }}}', maintainer_string)

    with open(landing_fn, 'w') as f:
        f.write(shell_data)

def populate_service_worker_template(shell_version, package_versions, landing_dir=defaults['landing_dir']):
    service_worker_data = ''
    landing_fn = os.path.join(landing_dir, 'service-worker.js')
    copy(os.path.join(script_dir, 'service-worker.js'), landing_fn)

    package_versions_string = ''
    for version in package_versions:
        package_versions_string += f"'{version}',"

    with open(landing_fn, 'r') as f:
        service_worker_data = f.read()

    service_worker_data = service_worker_data.replace('{{{ SHELL_VERSION }}}', shell_version)
    service_worker_data = service_worker_data.replace('{{{ PACKAGE_VERSIONS }}}', package_versions_string)

    with open(landing_fn, 'w') as f:
        f.write(service_worker_data)

def generate_splash_images(splash_image, npm_install=None, landing_dir=defaults['landing_dir']):
    # Generate splash image
    try:
        if os.path.exists(splash_image):
            npm_list = ''
            if npm_install == '_GLOBAL':
                npm_install = '-g'
                npm_list = '-g'
            elif npm_install is None:
                npm_install = ''
            else:
                npm_install = f'--prefix "{npm_install}"'
            os.system(f'npm list {npm_list} pwa-asset-generator || npm install {npm_install} pwa-asset-generator')
            os.system(f'pwa-asset-generator "{splash_image}" "{landing_dir}/assets" --background "black" --type "png" --splash-only')
    except BaseException as e:
        if splash_image is not None:
            raise e

def get_gtag(gtag):
    try:
        if os.path.exists(gtag):
            with open(gtag, 'r') as f:
                gtag = f.read()
        else:
            gtag = base64.b64decode(gtag).decode('utf-8')
    except BaseException as e:
        if gtag is not None:
            raise e
    return gtag

def parse_default(input, name):
    if (input == '_DEFAULT'):
        return defaults[name]
    return input

def main(version, skip_landing=False, package_versions=[], default_package_version=None, landing_dir=defaults['landing_dir'], splash_image=None, npm_install=None,
         gtag=None, url=defaults['url'], maintainer=None, maintainer_url=None,
         base_version=None, build_dir=defaults['build_dir'], data_dir=defaults['data_dir'], fwad=[], ewad=[], 
         out_zip=None, out_zip_no_assets=None):
    # Build parameters
    shell_version = str(int(time.time()))
    gtag = get_gtag(gtag)
    version_dir = os.path.join(landing_dir, 'data', version)
    if not default_package_version:
        default_package_version = version
    if not package_versions or not len(package_versions):
        package_versions = [default_package_version]

    # Make shell asset directory if non-existent
    os.makedirs(os.path.join(landing_dir, 'assets'), exist_ok=True)
    copy(os.path.join(script_dir, '..', 'srb2.png'), os.path.join(landing_dir, 'assets', 'srb2.png'))

    # If BASE version specified, then write the marker
    if base_version:
        os.makedirs(version_dir, exist_ok=True)
        with open(os.path.join(version_dir, '_BASE'), 'w') as f:
            f.write(base_version)

    # If build dir is specified, then copy the binary/JS over
    if build_dir is not None and os.path.isdir(build_dir):
        os.makedirs(version_dir, exist_ok=True)
        copy(os.path.join(build_dir, 'srb2.js'), os.path.join(version_dir, 'srb2.js'))
        copy(os.path.join(build_dir, 'srb2.wasm'), os.path.join(version_dir, 'srb2.wasm'))
    
    # If data dir is specified, then process data files
    if data_dir is not None and os.path.isdir(data_dir):
        for root, dirs, files in os.walk(data_dir):
            for file in files:
                fn = os.path.abspath(os.path.normpath(os.path.join(root, file)))
                # Takes two paths, A:\B\C\D and A:\B\C\D\E\F.txt and outputs E\F.txt
                fn_relative = fn.replace(f'{os.path.commonprefix([fn, os.path.abspath(data_dir)])}{os.path.sep}', '')
                version_fn = os.path.join(version_dir, fn_relative)
                os.makedirs(os.path.dirname(fn), exist_ok=True)
                if file in fwad or file in ewad:
                    convert_fwad(fn, version_fn, dump=os.path.join(version_dir, f'_{file}'), noindex=(file in ewad))
                else:
                    copy(fn, version_fn)

    # Generate landing page
    if not skip_landing:
        populate_shell_template(shell_version, gtag, package_versions=package_versions, default_package_version=default_package_version, landing_dir=landing_dir,
                                url=url, maintainer=maintainer, maintainer_url=maintainer_url)
    generate_splash_images(splash_image, npm_install=npm_install, landing_dir=landing_dir)

    # Generate service worker
    if not skip_landing:
        populate_service_worker_template(shell_version, package_versions, landing_dir=landing_dir)

    # Generate MD5
    if os.path.isdir(version_dir):
        dir_md5(version_dir)

    # Write version files
    with open(os.path.join(landing_dir, 'version-shell.txt'), 'w') as f:
        f.write(shell_version)
    with open(os.path.join(landing_dir, 'version-package.txt'), 'w') as f:
        f.write(default_package_version)

    # Generate ZIP
    if out_zip:
        with ZipFile(out_zip, 'w') as zip:
            # Iterate over all the files in directory
            for folderName, subfolders, filenames in os.walk(landing_dir):
                for filename in filenames:
                    fn = os.path.join(folderName, filename)
                    fn_relative = os.path.normpath(fn).replace(f'{os.path.commonprefix([os.path.normpath(fn), os.path.abspath(landing_dir)])}{os.path.sep}', '')
                    # Add file to zip
                    zip.write(fn, fn_relative)

    # Generate no-asset ZIP
    if out_zip_no_assets:
        with ZipFile(out_zip_no_assets, 'w') as zip:
            # Iterate over all the files in directory
            for folderName, subfolders, filenames in os.walk(landing_dir):
                for filename in filenames:
                    if 'data' in folderName:
                        if not ('.js' in filename or '.wasm' in filename 
                                or '_BASE' in filename or '_FULLINSTALL' in filename
                                or '_INSTALL' in filename or '_PERSISTENT' in filename
                                or '_REQUIRED' in filename or '_STARTUP' in filename):
                            continue
                    fn = os.path.join(folderName, filename)
                    fn_relative = os.path.normpath(fn).replace(f'{os.path.commonprefix([os.path.normpath(fn), os.path.abspath(landing_dir)])}{os.path.sep}', '')
                    # Add file to zip
                    zip.write(fn, fn_relative)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Prepare a deployable Emscripten package for web.')
    # Package meta
    parser_package = parser.add_argument_group('Package Info')
    parser_package.add_argument('version', type=str, help='Name of version to write game binary and assets to.')
    parser_package.add_argument('--skip-landing', action='store_true', default=False, help='Skip generating the landing page.')
    parser_package.add_argument('--package-versions', type=str, nargs='+', default=[], help='List of versions to make playable through the landing page. Defaults to {version}.')
    parser_package.add_argument('--default-package-version', type=str, help='Default version to play through the landing page. Defaults to {version}.')
    parser_package.add_argument('--landing-dir', type=str, default=defaults['landing_dir'], help='Directory to complete package. Default dir: {repo_dir}/emscripten/landing')
    parser_package.add_argument('--splash-image', type=str, help='Path to base image to generate Apple splash images. If you specify this argument, you must have NPM installed. The package "pwa-asset-generator" will be installed.')
    parser_package.add_argument('--npm-install', type=str, help='Location to install "pwa-asset-generator". If blank, will install into "{cwd}/node_modules". If "_GLOBAL", will install globally.')
    parser_package.add_argument('--gtag', type=str, help='Path to file from which to read Google Analytics GTAG for insertion into landing page. Or, you may specify a BASE64-encoded string of the GTAG.')
    parser_package.add_argument('--url', type=str, default=defaults['url'], help=f'Base URL where you intend to deploy. Default: {defaults["url"]}')
    parser_package.add_argument('--maintainer', type=str, help='Name of responsible party for maintaining the web service. This name is displayed at the bottom of the web page.')
    parser_package.add_argument('--maintainer-url', type=str, help='URL to web site of the responsible party.')
    # Game data
    parser_data = parser.add_argument_group('Game Data')
    parser_data.add_argument('--base-version', type=str, help='Name of version to use as a game asset base.')
    parser_data.add_argument('--build-dir', type=str, default=defaults['build_dir'], help='Directory to copy game binary from. Default dir: {repo_dir}/bin/Emscripten/Release')
    parser_data.add_argument('--skip-build', action='store_true', default=False, help='Skip copying game binary to landing.')
    parser_data.add_argument('--data-dir', type=str, default=defaults['data_dir'], help='Directory to copy game assets from. Default dir: {repo_dir}/emscripten/data')
    parser_data.add_argument('--skip-data', action='store_true', default=False, help='Skip copying game assets to landing.')
    parser_data.add_argument('--fwad', type=str, nargs='+', default=[], help='Any wadfile names to convert to FWAD using numbered lump filenames.')
    parser_data.add_argument('--ewad', type=str, nargs='+', default=[], help='Any wadfile names to convert to FWAD using non-numbered lump filenames.')
    # Output
    parser_output = parser.add_argument_group('Output')
    parser_output.add_argument('--out-zip', type=str, help='Name of ZIP to output, including assets; optional.')
    parser_output.add_argument('--out-zip-no-assets', type=str, help='Name of ZIP to output, without game assets; optional.')

    args = parser.parse_args()
    args_dict = vars(args)
    if args_dict.pop('skip_build'):
        args_dict['build_dir'] = None
    if args_dict.pop('skip_data'):
        args_dict['data_dir'] = None

    main(**args_dict)
