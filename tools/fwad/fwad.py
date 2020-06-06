# SONIC ROBO BLAST 2
# -----------------------------------------------------------------------------
# Copyright (C) 1993-1996 by id Software, Inc.
# Copyright (C) 1998-2000 by DooM Legacy Team.
# Copyright (C) 1999-2020 by Sonic Team Junior.

# This program is free software distributed under the
# terms of the GNU General Public License, version 2.
# See the 'LICENSE' file for more details.
# -----------------------------------------------------------------------------
# / \file  fwad.py
# / \brief Generates flat-file WAD from existing wadfile.

import os
import collections
import struct
import hashlib

# Taken from https://github.com/johndoe31415/wadcode/blob/master/WADFile.py
# Johannes Bauer <JohannesBauer@gmx.de>

class NamedStruct(object):
	def __init__(self, fields, struct_extra = '<'):
		struct_format = struct_extra + (''.join(fieldtype for (fieldtype, fieldname) in fields))
		self._struct = struct.Struct(struct_format)
		self._collection = collections.namedtuple('Fields', [ fieldname for (fieldtype, fieldname) in fields ])

	@property
	def size(self):
		return self._struct.size

	def pack(self, data):
		fields = self._collection(**data)
		return self._struct.pack(*fields)

	def unpack(self, data):
		values = self._struct.unpack(data)
		fields = self._collection(*values)
		return fields

	def unpack_head(self, data):
		return self.unpack(data[:self._struct.size])

	def unpack_from_file(self, f, at_offset = None):
		if at_offset is not None:
			f.seek(at_offset)
		data = f.read(self._struct.size)
		return self.unpack(data)

class FWAD(object):
	_WAD_HEADER = NamedStruct((
		('4s', 'magic'),
		('l', 'numlumps'),
		('l', 'infotableofs'),
	))

	_FILE_ENTRY = NamedStruct((
		('l', 'position'),
		('l', 'size'),
		('8s', 'name'),
	))

	_Lump = collections.namedtuple('Lump', ['name','data'])

	def __init__(self, fn=None):
		self.lumps = []
		self.fn = fn
		if fn:
			self.load(fn)

	def load(self, fn):
		with open(fn, 'rb') as f:
			header = self._WAD_HEADER.unpack_from_file(f)
			assert(header.magic == b'IWAD'
				   or header.magic == b'PWAD'
				   or header.magic == b'SDLL')
			f.seek(header.infotableofs)
			for numlumps in range(header.numlumps):
				fileinfo = self._FILE_ENTRY.unpack_from_file(f)
				name = fileinfo.name.rstrip(b'\x00').decode('latin1')
				cur_pos = f.tell()
				f.seek(fileinfo.position)
				data = f.read(fileinfo.size)
				f.seek(cur_pos)
				resource = self._Lump(name=name, data=data)
				self.lumps.append(resource)
		self.fn = fn
		return self

	def dump(self, path = None, md5 = False, use_index = True):
		# Dump lumps to files in subdir: ./_{wadfilename}
		dump_path = path or os.path.join(os.path.dirname(self.fn) or '.', f'_{os.path.basename(self.fn)}')
		os.makedirs(dump_path, exist_ok=True)
		for i, lump in enumerate(self.lumps):
			index = f'{i}_' if use_index else ''
			dump_name = os.path.join(dump_path, f'{index}{lump.name}')
			with open(dump_name, 'wb') as f:
				f.write(lump.data)
			if md5:
				with open(f'{dump_name}.md5', 'w') as f:
					f.write(hashlib.md5(lump.data).hexdigest())
	
	def save_fwad(self, fn, md5 = False, use_index = True):
		# Write the FWAD: Entries list the lump size
		with open(fn, 'wb') as f:
			directory_offset = self._WAD_HEADER.size
			header = self._WAD_HEADER.pack({
				'magic': b'FWAD' if use_index else b'EWAD',
				'numlumps': len(self.lumps),
				'infotableofs': self._WAD_HEADER.size,
			})
			f.write(header)

			data_offset = directory_offset + (len(self.lumps) * self._FILE_ENTRY.size)
			for lump in self.lumps:
				# Write data as lump size
				data = str(len(lump.data)).encode('ascii')
				file_entry = self._FILE_ENTRY.pack({
					'position': data_offset,
					'size':	len(data),
					'name':	lump.name.strip()[0:min(len(lump.name.strip()), 8)].ljust(8, '\0').encode('ascii'),
				})
				f.write(file_entry)
				data_offset += len(data)
			for lump in self.lumps:
				data = str(len(lump.data)).encode('ascii')
				f.write(data)
		if md5:
			with open(fn, 'rb') as e:
				with open(f'{fn}.md5', 'w') as f:
					f.write(hashlib.md5(e.read()).hexdigest())

def fwad_fn(fn):
	split_name = os.path.splitext(fn)
	ext = f'.{split_name[1]}' if len(split_name) > 1 else ''
	fwad_name = f'{split_name[0]}-f{ext}'
	return os.path.join(os.path.dirname(fn) or '.', fwad_name)

def convert_fwad(in_file, out_file, dump=None, md5=False, noindex=False):
	a = FWAD(in_file)
	a.dump(dump, md5=md5, use_index=(not noindex))
	a.save_fwad(out_file, md5=md5, use_index=(not noindex))

if __name__ == '__main__':
	import argparse
	parser = argparse.ArgumentParser(description='Convert a WAD to a flat-file WAD.')
	parser.add_argument('in_file', type=str, help='Input WAD file.')
	parser.add_argument('out_file', type=str, help='Output FWAD file.')
	parser.add_argument('-d', '--dump', type=str, help='Directory to output lump files. Defaults to subdirectory "./_in_file" where the input file is located.')
	parser.add_argument('--md5', action='store_true', default=False, help='Output MD5 digests of lumps and output WAD as [file].md5')
	parser.add_argument('--noindex', action='store_true', default=False, help='Do not name lump files with index number.')

	args = parser.parse_args()
	
	convert_fwad(**vars(args))
