# SONIC ROBO BLAST 2
# -----------------------------------------------------------------------------
# Copyright (C) 1993-1996 by id Software, Inc.
# Copyright (C) 1998-2000 by DooM Legacy Team.
# Copyright (C) 1999-2020 by Sonic Team Junior.

# This program is free software distributed under the
# terms of the GNU General Public License, version 2.
# See the 'LICENSE' file for more details.
# -----------------------------------------------------------------------------
# / \file  md5.py
# / \brief Generates MD5 hashes for files in a directory tree and saves to *.md5

import os
import hashlib

def file_md5(fn):
    if os.path.isfile(fn) and not fn.endswith('.md5'):
        with open(fn, 'rb') as e:
            with open(f'{fn}.md5', 'w') as f:
                f.write(hashlib.md5(e.read()).hexdigest())

def dir_md5(in_path):
    for root, dirs, files in os.walk(in_path):
        for file in files:
            fn = os.path.join(root, file)
            file_md5(fn)

def main(in_path):
    if os.path.isdir(in_path):
        dir_md5(in_path)

    elif os.path.isfile(in_path):
        file_md5(in_path)

    else:
        raise ValueError('in_path is not a valid directory or file.')

if __name__ == '__main__':
	import argparse
	parser = argparse.ArgumentParser(description='Write MD5 hashes in a directory tree.')
	parser.add_argument('in_path', type=str, help='Input directory or file.')

	args = parser.parse_args()
	
	main(args.in_path)
