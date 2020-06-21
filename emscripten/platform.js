// SONIC ROBO BLAST 2
// -----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------
/// \file platform.js
/// \brief Program code for assisting an emscripten build. Includes Module object.

////////////////////////////////
// Version Check - For PWA
////////////////////////////////

let AuthorUpdateShown = false;

var CheckVersion = async () => {
	if (document.body.classList.contains('calledRun'))
		return;

	try {
		////////////////////////////////
		// Check Web Version
		////////////////////////////////

		let response = await fetch("version-shell.txt");
		let data = await response.text();
		
		console.log(`SRB2 Web Version: {{{ SHELL_VERSION }}}`);
		if (data.trim() && data.trim() !== "{{{ SHELL_VERSION }}}") {
			console.log(`New Web Version Found: ${data}`);
			// Don't refresh page more than once in a row.
			let updateCount = localStorage.getItem('srb2web_update');
			if (IsStandalone() && (!updateCount || !parseInt(updateCount))) {
				localStorage.setItem('srb2web_update', (1).toString());
				window.location.reload(true);
				return;
			} else {
				if (document.getElementById('newVersion'))
					document.getElementById('newVersion').outerHTML = `<p style="font-size:xx-small;">Update Version: <a href="#" onclick="window.location.reload(true);return false;">${data}</a></p>`;
			}
		}
		localStorage.setItem('srb2web_update', (0).toString());
		
		////////////////////////////////
		// Check Author Updates
		////////////////////////////////

		// If this app were to be decentralized, I would still to be able
		// to communicate updates. Line 1 is the update ID, everything else
		// goes in an alert. Show the alert on two page loads.
		// Put new updates in a new filename.

		if (!AuthorUpdateShown) {
			try {
				// get data
				let authorUrl = 'https://raw.githubusercontent.com/mazmazz/SRB2-emscripten/emscripten-new/emscripten/author-update.txt';
				let authorResponse = await fetch(authorUrl, {mode: 'cors'});
				if (!authorResponse.ok)
					throw(`Author update check status: ${authorResponse.status}`);
				data = await authorResponse.text();
				data = data.replace('\r\n','\n');
				let linePos = data.indexOf('\n');

				// process data
				let updateId = data.substr(0, linePos > -1 ? linePos : data.length);
				let updateText = data.substr(linePos+1 < data.length ? linePos+1 : data.length, data.length);

				if (!updateId.trim() || !updateText.trim())
					throw('Invalid author update, updateId or updateText was blank');

				// compare against stored info
				let updateCount = localStorage.getItem('srb2web_authorUpdate');
				updateCount = parseInt(updateCount);
				if (!updateCount)
					updateCount = 0;
				let updateStoredId = localStorage.getItem('srb2web_authorUpdate_id');
				if (updateId !== updateStoredId)
					updateCount = 0;
				
				// show the update?
				if (!AuthorUpdateShown) {
					if (updateCount++ < 2) {
						AuthorUpdateShown = true; // don't show again for this page load
						alert(updateText);
					}
				}

				// store variables
				localStorage.setItem('srb2web_authorUpdate_id', updateId);
				localStorage.setItem('srb2web_authorUpdate', updateCount.toString());
			} catch (err) { 
				console.log('CheckVersion: author update check', err);
			}
		}

		////////////////////////////////
		// Check default program version
		////////////////////////////////

		// Don't check program version more than once a page load
		let updateCount = localStorage.getItem('srb2program_update');
		if (!updateCount || !parseInt(updateCount))
			response = await fetch("version-package.txt");
		else
			throw("Already checked program version.");
		
		data = await response.text();

		let previousDefaultPackageVersion = localStorage.getItem('srb2program_defaultversion');
		console.log(`SRB2 Default Program Version: {{{ PACKAGE_VERSION }}}`);
		if (data.trim() && previousDefaultPackageVersion && data.trim() !== previousDefaultPackageVersion) {
			console.log(`New Default Program Version Found: ${data}`);
			if (PackageVersionElement.length > 1 && confirm(`SRB2 Version ${data} is released! Do you want to switch to the new version?`)) {
				for (let i = 0; i < PackageVersionElement.length; i++) {
					if (PackageVersionElement.options[i].value === data) {
						PackageVersionElement.selectedIndex = i;
						SaveFormToStorage(ControlsFormElement);
						alert(`The new version will run when you ${UserAgentIsMobile() ? 'tap' : 'click'} "Play".`);
						break;
					}
				}
			}
		}
		localStorage.setItem('srb2program_defaultversion', data.trim());
	} catch (err) {
		console.log(`Error Checking New Version:`,err);
		localStorage.setItem('srb2web_update', (0).toString());
		throw err;
	}
};

// reset on page load
localStorage.setItem('srb2program_update', (0).toString());
window.addEventListener('load', CheckVersion, {once: true});
window.addEventListener('focus', CheckVersion, false);

////////////////////////////////
// Utilities
////////////////////////////////

var UserAgentIsiOS = () => {
	var ua = window.navigator.userAgent;
	var iOS = !!ua.match(/iPad/i) || !!ua.match(/iPhone/i) || !!ua.match(/iPod/i);
	var webkit = !!ua.match(/WebKit/i);
	var iOSSafari = iOS && webkit && !ua.match(/CriOS/i) && !ua.match(/FxiOS/i);
	var iPad13 = (/iPad|iPhone|iPod/.test(navigator.platform) ||
		(navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1)) &&
		!window.MSStream;
	return iOSSafari || iPad13;
};

var GetiOSVersion = () => {
	const ua = navigator.userAgent;
	if (/(iPhone|iPod|iPad)/i.test(ua))
		return ua.match(/OS [\d_]+/i)[0].substr(3).split('_').map(n => parseInt(n));
	// iPad 13
	else if (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1 && !window.MSStream)
		return ua.match(/Version\/[\d.]+/i)[0].substr(3).split('.').map(n => parseInt(n));
	return [0];
};

var UserAgentIsiPhone = () => /iPhone|iPod/.test(navigator.userAgent);

var IsStandalone = () => (
	(window.matchMedia('(display-mode: standalone)').matches) || 
		(("standalone" in window.navigator) &&
		 window.navigator.standalone)
);

// https://stackoverflow.com/a/11381730
var UserAgentIsMobile = () => {
	let check = false;
	(function(a){if(/(android|bb\d+|meego).+mobile|avantgo|bada\/|blackberry|blazer|compal|elaine|fennec|hiptop|iemobile|ip(hone|od)|iris|kindle|lge |maemo|midp|mmp|mobile.+firefox|netfront|opera m(ob|in)i|palm( os)?|phone|p(ixi|re)\/|plucker|pocket|psp|series(4|6)0|symbian|treo|up\.(browser|link)|vodafone|wap|windows ce|xda|xiino/i.test(a)||/1207|6310|6590|3gso|4thp|50[1-6]i|770s|802s|a wa|abac|ac(er|oo|s\-)|ai(ko|rn)|al(av|ca|co)|amoi|an(ex|ny|yw)|aptu|ar(ch|go)|as(te|us)|attw|au(di|\-m|r |s )|avan|be(ck|ll|nq)|bi(lb|rd)|bl(ac|az)|br(e|v)w|bumb|bw\-(n|u)|c55\/|capi|ccwa|cdm\-|cell|chtm|cldc|cmd\-|co(mp|nd)|craw|da(it|ll|ng)|dbte|dc\-s|devi|dica|dmob|do(c|p)o|ds(12|\-d)|el(49|ai)|em(l2|ul)|er(ic|k0)|esl8|ez([4-7]0|os|wa|ze)|fetc|fly(\-|_)|g1 u|g560|gene|gf\-5|g\-mo|go(\.w|od)|gr(ad|un)|haie|hcit|hd\-(m|p|t)|hei\-|hi(pt|ta)|hp( i|ip)|hs\-c|ht(c(\-| |_|a|g|p|s|t)|tp)|hu(aw|tc)|i\-(20|go|ma)|i230|iac( |\-|\/)|ibro|idea|ig01|ikom|im1k|inno|ipaq|iris|ja(t|v)a|jbro|jemu|jigs|kddi|keji|kgt( |\/)|klon|kpt |kwc\-|kyo(c|k)|le(no|xi)|lg( g|\/(k|l|u)|50|54|\-[a-w])|libw|lynx|m1\-w|m3ga|m50\/|ma(te|ui|xo)|mc(01|21|ca)|m\-cr|me(rc|ri)|mi(o8|oa|ts)|mmef|mo(01|02|bi|de|do|t(\-| |o|v)|zz)|mt(50|p1|v )|mwbp|mywa|n10[0-2]|n20[2-3]|n30(0|2)|n50(0|2|5)|n7(0(0|1)|10)|ne((c|m)\-|on|tf|wf|wg|wt)|nok(6|i)|nzph|o2im|op(ti|wv)|oran|owg1|p800|pan(a|d|t)|pdxg|pg(13|\-([1-8]|c))|phil|pire|pl(ay|uc)|pn\-2|po(ck|rt|se)|prox|psio|pt\-g|qa\-a|qc(07|12|21|32|60|\-[2-7]|i\-)|qtek|r380|r600|raks|rim9|ro(ve|zo)|s55\/|sa(ge|ma|mm|ms|ny|va)|sc(01|h\-|oo|p\-)|sdk\/|se(c(\-|0|1)|47|mc|nd|ri)|sgh\-|shar|sie(\-|m)|sk\-0|sl(45|id)|sm(al|ar|b3|it|t5)|so(ft|ny)|sp(01|h\-|v\-|v )|sy(01|mb)|t2(18|50)|t6(00|10|18)|ta(gt|lk)|tcl\-|tdg\-|tel(i|m)|tim\-|t\-mo|to(pl|sh)|ts(70|m\-|m3|m5)|tx\-9|up(\.b|g1|si)|utst|v400|v750|veri|vi(rg|te)|vk(40|5[0-3]|\-v)|vm40|voda|vulc|vx(52|53|60|61|70|80|81|83|85|98)|w3c(\-| )|webc|whit|wi(g |nc|nw)|wmlb|wonu|x700|yas\-|your|zeto|zte\-/i.test(a.substr(0,4))) check = true;})(navigator.userAgent||navigator.vendor||window.opera);
	return check;
};

var UserAgentIsAndroid = () => /Android/.test(navigator.userAgent);

var ResizeDimensions = (x, y, resizeHeight) => {
	let portrait = (x < y);
	let width = Math.max(x, y);
	let height = Math.min(x, y);
	let target = Math.max(resizeHeight, (portrait ? 320 : 200)); // BASEVIDHEIGHT
	let factor;

	if (!resizeHeight)
		return {x:x, y:y};
	
	factor = target / height;
	if (width * factor < 320) // BASEVIDWIDTH
		factor = 320 / width;

	width *= factor;
	height *= factor;

	if (portrait)
	{
		x = Math.ceil(height);
		y = Math.ceil(width);
	}
	else
	{
		x = Math.ceil(width);
		y = Math.ceil(height);
	}
	return {x:x, y:y};
};

////////////////////////////////
// Base FS Functions
////////////////////////////////

var GetBasenameFromPath = (path) => path.split('\\').pop().split('/').pop();
var GetDirnameFromPath = (path) => {
	let arr = path.split('\\').pop().split('/');
	arr.pop();
	return arr.join('/');
};

let TypedArrayToBuffer = (array) => array.buffer.slice(array.byteOffset, array.byteLength + array.byteOffset);

// iOS Safari does not implement Blob.arrayBuffer(), so let's
// do it ourselves.
// https://gist.github.com/hanayashiki/8dac237671343e7f0b15de617b0051bd
function MyArrayBuffer () {
	// this: File or Blob
	return new Promise((resolve) => {
		let fr = new FileReader();
		fr.onload = () => {
			resolve(fr.result);
		};
		fr.readAsArrayBuffer(this);
	})
}

var ImplementArrayBuffer = () => {
	if ('File' in self)
		File.prototype.arrayBuffer = File.prototype.arrayBuffer || MyArrayBuffer;
	if ('Blob' in self)
		Blob.prototype.arrayBuffer = Blob.prototype.arrayBuffer || MyArrayBuffer;
};

ImplementArrayBuffer();

var InitializeFS = () => {
	FS.mkdirTree('/addons');
	FS.symlink('/home/web_user/.srb2', '/addons/.srb2');
	FS.symlink('/home/web_user/.srb2', '/addons/userdata');
	FS.mount(IDBFS, {}, '/home/web_user');
	return (new Promise((resolve, reject) => {
		FS.syncfs(true, (err) => {
			console.log("SyncFS done");
			console.log(err);
			resolve();
		});
	}));
};

var WriteFS = (baseDir, path, data) => {
	if (data instanceof ArrayBuffer)
		data = new Uint8Array(data);
	// split path to base dir and filename
	if (path.includes('/') || path.includes('\\'))
		baseDir = `${baseDir}/${GetDirnameFromPath(path)}`;
	fn = GetBasenameFromPath(path);
	// check for symlinks
	let parents = '/';
	baseDir.split('/').forEach((name) => {
		if (name.length) {
			let mode = 0;
			try { mode = FS.lstat(`${parents}${name}`)['mode']; } catch(err) { console.log('WriteFS(): lstat info'); console.log(err); }
			if (FS.isLink(mode))
				parents = `${FS.readlink(`${parents}${name}`)}/`;
			else
				parents += `${name}/`;
		}
	});
	baseDir = parents.substring(0, parents.length-1);
	console.log(`WriteFS(): Writing ${baseDir}/${fn}: ${data.byteLength} bytes`);
	// attempt write
	try { FS.mkdirTree(parents); } catch(err) { console.log('WriteFS(): mkdirTree info'); console.log(err); }
	try { FS.unlink(`${baseDir}/${fn}`); } catch(err) { console.log('WriteFS(): unlink info'); console.log(err); }
	FS.createDataFile(baseDir, fn, data, true, true);
	return Promise.resolve(true);
};

var SyncFS = (populate = false) => {
	// commit to persistent storage
	return (new Promise((resolve, reject) => {
		if (typeof FS !== 'undefined')
			FS.syncfs(populate, (err) => {
				console.log('Synced persistent storage');
				console.log(err);
			});
		return resolve();
	}));
};

var DownloadFS = (downloadPath, manageLoop = true, callback = null) => {
	// Emscripten file store is an IndexedDB, name /home/web_user, object FILE_DATA,
	// version 21.
	// idb-keyval doesn't take in a version parameter (forces 1),
	// so use mazmazz/idb-keyval@idb-version to specify a version.
	// Files are listed as [fullPath]: {timestamp, mode, contents}
	let customStore = new idbKeyval.Store('/home/web_user', 'FILE_DATA', 21);
	if (typeof downloadPath === 'undefined')
		downloadPath = '/home/web_user/.srb2';

	if (manageLoop && StartedMainLoop)
		PauseLoop();

	return SyncFS()
	.then(_ => {
		return idbKeyval.keys(customStore);
	})
	.then(keys => {
		let promises = [];
		let zip = new JSZip();
		keys.forEach(key => {
			if (key.includes(downloadPath))
				promises.push(
					idbKeyval.get(key, customStore)
					.then(val => {
						// JSZip creates nested folders automatically.
						// Just iterate over files.
						if ('contents' in val && val.contents)
							return zip.file(key.replace('/home/web_user/',''), TypedArrayToBuffer(val.contents), {date: new Date(val.timestamp)});
					})
				);
		});
		return Promise.all(promises)
		.then(_ => zip);
	})
	.then(zip => zip.generateAsync({type:'blob'}))
	.then(blob => Promise.resolve(saveAs(blob, 'srb2-data.zip')))
	.catch(err => { console.log(`DownloadFS: ${err}`); alert(`Error downloading user data: ${err}`); })
	.finally(_ => {
		if (typeof callback === 'function')
			callback();
		if (manageLoop && StartedMainLoop)
			ResumeLoop();
	});
};

var DeleteFS = (deletePath, manageLoop = true, callback = null) => {
	// Emscripten file store is an IndexedDB, name /home/web_user, object FILE_DATA,
	// version 21.
	// idb-keyval doesn't take in a version parameter (forces 1),
	// so use mazmazz/idb-keyval@idb-version to specify a version.
	// Files are listed as [fullPath]: {timestamp, mode, contents}
	if (typeof deletePath === 'undefined')
		throw 'DeleteFS: Must specify a path';

	if (FS) {
		try {
			FS.unlink(`${deletePath}`);
		} catch (e) { 
			console.error(`DeleteFS: ${deletePath} - `, e);
		}
		
		return SyncFS().then(_ => {
			if (typeof callback === 'function')
				callback();
			if (manageLoop && StartedMainLoop)
				ResumeLoop();
		});
	}
	
	let customStore = new idbKeyval.Store('/home/web_user', 'FILE_DATA', 21);

	if (manageLoop && StartedMainLoop)
		PauseLoop();

	return SyncFS()
	.then(_ => {
		return idbKeyval.keys(customStore);
	})
	.then(keys => {
		let promises = [];
		keys.forEach(key => {
			if (key.includes(deletePath))
				promises.push(idbKeyval.del(key, customStore));
		});
		return Promise.all(promises)
	})
	.then(_ => SyncFS(true)) // commit changes to the memory FS
	.catch(err => console.log(`DeleteFS: ${err}`))
	.finally(_ => {
		if (typeof callback === 'function')
			callback();
		if (manageLoop && StartedMainLoop)
			ResumeLoop();
	});
};

////////////////////////////////
// Program Data
////////////////////////////////

// Files to download on first run
var InstallFiles = [];

// Files to download on full install (currently unused)
//var FullInstallFiles = [];

// Don't delete these files on lump unload
var PersistentLumpFiles = [];

// Files to place in FS on game startup
var StartupFiles = [];

// Files that must be downloaded before game startup
var RequiredFiles = [];

// Store of version bases
var VersionBases = {};

var CompositeKey = (fn, version) => `data/${version}/${fn}`;

var StripLeadingSeparators = (fn) => fn.replace(/^\/*/, '');

var ResetProgramData = async (manageLoop = true, callback = null) => {
	// Delete specific files in IndexedDB /home/web_user -> FILE_DATA
	// and delete everything in EM_PRELOAD_CACHE -> METADATA, PACKAGES

	let customStore = new idbKeyval.Store('/home/web_user', 'FILE_DATA', 21);
	let customStoreData = new idbKeyval.Store('SRB2_DATA', 'FILES', 1);
	let customStoreEmMetadata = new idbKeyval.Store('EM_PRELOAD_CACHE', 'METADATA', 1);
	let customStoreEmPackages = new idbKeyval.Store('EM_PRELOAD_CACHE', 'PACKAGES', 1);

	if (manageLoop && StartedMainLoop)
		PauseLoop();

	// Let these fail silently in case the keys do not exist.
	var silentCatch = _ => {return;};

	await idbKeyval.clear(customStoreEmMetadata).catch(silentCatch);
	await idbKeyval.clear(customStoreEmPackages).catch(silentCatch);
	// TODO reliable recursive way to clear multiple versions of data?
	// because later versions may use older versions as a BASE
	await idbKeyval.clear(customStoreData).catch(silentCatch);
	// We don't store game assets in the user folder
	await idbKeyval.del('/home/web_user/.srb2/music.dta', customStore).catch(silentCatch);

	if (typeof callback === 'function')
		callback();
	if (manageLoop && StartedMainLoop)
		ResumeLoop();
	
	return Promise.resolve();
};

var InstallProgramFileReferences = async (name, version, fullBase) => {
	let customStore = new idbKeyval.Store('SRB2_DATA', 'FILES', 1);
	// Slice all parent bases of the current version, so we don't
	// overwrite parents' files.
	let fullBase2 = fullBase.slice(0, fullBase.indexOf(version));

	return Promise.all(fullBase2.map(async base => {
		if (base && base !== version) {
			let baseFile = await idbKeyval.get(CompositeKey(name, base), customStore);
			if (!(baseFile instanceof Object))
				baseFile = {};
			if (!('contents' in baseFile)) {
				if (!('base' in baseFile && baseFile['base'] != version)) {
					baseFile['base'] = version;
					console.log(`InstallProgramFileReferences: Logging base ${version} for ${name} (${base})`);
					return idbKeyval.set(CompositeKey(name, base), baseFile, customStore);
				} else
					return Promise.resolve(); // nothing to update
			} 
			// IF AN ENTRY EXISTS IN A PARENT BASE: Be wary. Don't log anything,
			// in case the parent file was in error.
			return Promise.reject('While logging parent base references, parent ${base} already has a file.');
		}
	}));
};

var InstallProgramFile = async (name, version, md5 = null, fullBase = []) => {
	let customStore = new idbKeyval.Store('SRB2_DATA', 'FILES', 1);

	try {
		let remoteFile = await fetch(`data/${version}/${name}`);

		if (!remoteFile.ok)
			throw remoteFile;
		
		remoteFile = await remoteFile.arrayBuffer();
		remoteFile = new Uint8Array(remoteFile);

		// Log a reference to this version in all the parent bases
		await InstallProgramFileReferences(name, version, fullBase);

		// Log to persistent storage
		let idbFile = await idbKeyval.get(CompositeKey(name, version), customStore);
		if (!idbFile)
			idbFile = {};
		idbFile['contents'] = remoteFile;
		idbFile['md5'] = md5;
		await idbKeyval.set(CompositeKey(name, version), idbFile, customStore);
		console.log(`Downloaded ${name} (${version}) from server.`);
		return Promise.resolve(idbFile);
	} catch (e) {
		// fetch connection error or other throw from above
		throw `CheckInstallProgramFile: data/${version}/${name} - ${e}`;
	}
};

var CheckInstallProgramFile = async (name, version, base = [], fullBase = [], checkMd5IfFileExists = false) => {
	// Every data file on the server has a corresponding *.md5 file.
	// Check the server for a file in the given version.
	// If it doesn't exist in the given version, then try the "base" version.

	if (typeof version === 'undefined' || !version) {
		if (base && base.length)
			return CheckInstallProgramFile(name, base.shift(), base, fullBase, checkMd5IfFileExists);
		else
			throw `CheckInstallProgramFile: ${name} - No version to fetch from!`;
	}

	let customStore = new idbKeyval.Store('SRB2_DATA', 'FILES', 1);

	// First, see if we can get a base reference in IDB
	try {
		let localFile = await idbKeyval.get(CompositeKey(name, version), customStore);
		if (localFile && localFile instanceof Object) {
			if ('base' in localFile)
				return await CheckInstallProgramFile(name, localFile['base'], [], fullBase, checkMd5IfFileExists);
			else if (!checkMd5IfFileExists) {
				console.log(`Retrieved ${name} (${version}) from storage.`);
				// Don't log a base reference (InstallProgramFileReferences),
				// because we want to verify with server before doing so.
				return Promise.resolve(localFile);
			}
		}
	} catch (e) { } // fail silently; oh well, we tried.

	try {
		// Check server's MD5 file for the current version.
		let remoteMd5 = await fetch(`data/${version}/${name}.md5`);
		if (!remoteMd5.ok) {
			// Server's MD5 was not found: check the "base" version for the file.
			if (base && base.length && base[0])
				return await CheckInstallProgramFile(name, base.shift(), base, fullBase, checkMd5IfFileExists);
			else if (!RequiredFiles.includes(name))
				return Promise.resolve(); // shrug, we don't need this file.
			else
				throw 'Cannot retrieve MD5';
		} else {
			// Server's MD5 was found: compare it to our local MD5 for the file.
			let idbFile = await idbKeyval.get(CompositeKey(name, version), customStore);
			remoteMd5 = await remoteMd5.text();

			if (idbFile && 'md5' in idbFile
					&& remoteMd5 && remoteMd5 == idbFile['md5']) {
				// The MD5's match: we're done, don't download anything.
				// Log a reference to this version in all the parent bases
				await InstallProgramFileReferences(name, version, fullBase);
				console.log(`Retrieved ${name} (${version}) from storage.`);
				return Promise.resolve(idbFile);
			}
			else if (remoteMd5 && remoteMd5.length == 32) { // sanity check
				// The MD5's do not match: store the server's MD5, and download
				// the file for the current version.
				return await InstallProgramFile(name, version, remoteMd5, fullBase);
			} else {
				// Edge case where remote MD5 is not valid: see if there's an older file.
				if (base && base.length && base[0])
					return await CheckInstallProgramFile(name, base.shift(), base, fullBase, checkMd5IfFileExists);
				else if (!RequiredFiles.includes(name))
					return Promise.resolve(); // shrug, we don't need this file.
				else
					throw 'Cannot retrieve remote MD5.';
			}
		}
	} catch (e) {
		// fetch() connection error, or thrown from above code.
		throw `CheckInstallProgramFile: data/${version}/${name}.md5 - ${e}`;
	}
};

var CheckInstallFileList = async (fileList, version = '{{{ PACKAGE_VERSION }}}', checkServer = false, progressCallback = null, finishedCallback = null) => {
	let bases = await GetVersionBases(version);
	let files = [...fileList];
	// Using es6-promise-pool to rate-limit file requests.
	// This works like a loop: when the Pool requests a promise,
	// it produces promises based on the count index.
	let count = 0;
	let promiseProducer = () => {
		if (count < files.length) {
			let basesRecursive = [...bases]; // first entry is our own version
			return CheckInstallProgramFile(files[count++], basesRecursive.shift(), basesRecursive, [...bases], checkServer)
			.then(_ => { 
				if (typeof progressCallback === 'function')
					progressCallback(files[count], count, files.length);
			});
		} else
			return null;
	};

	// Prime the progress callback with the first file
	if (typeof progressCallback === 'function')
		progressCallback(files[count], count, files.length);

	let pool = new PromisePool(promiseProducer, 3);

	return pool.start()
	.then(_ => {
		if (typeof finishedCallback === 'function')
			finishedCallback();
	})
	.catch(e => { console.error(`CheckInstallFileList: ${e}`); throw e; });
};

var GetVersionFileLists = async (version = '{{{ PACKAGE_VERSION }}}') => {
	let fileLists = {'_INSTALL': InstallFiles,
									 //'_FULLINSTALL': FullInstallFiles,
									 '_PERSISTENT': PersistentLumpFiles,
									 '_STARTUP': StartupFiles,
									 '_REQUIRED': RequiredFiles};
	// Download all file lists
	try {
		await CheckInstallFileList(Object.keys(fileLists), version, true, null, null);
	} catch (e) { console.error('GetVersionFileLists error: ^^^^^'); }

	// Populate local file lists
	// This is inefficent, but pull the files we just downloaded
	// Hopefully we got them all, so we just retrieve them from the IDB.
	await Promise.all(Object.keys(fileLists).map(async (name) => {
		try {
			fileLists[name].length = 0; // clear array, keep reference
			let file = await RetrieveInstalledFile(name, version, false);
			if (file instanceof Object && 'contents' in file && file.contents) {
				// Convert to text file
				let textList = new TextDecoder("utf-8").decode(file.contents);
				textList = textList.replace('\r\n','\n');
				textLines = textList.split('\n');
				// Add entry to file list
				textLines.forEach(line => {
					let lineTrim = line.trim();
					if (lineTrim && !lineTrim.startsWith('//'))
						fileLists[name].push(lineTrim);
				});
			}
		} catch (e) {
			console.error(`GetVersionFileLists: Could not populate list ${name}`, e);
		}
		return name;
	}));

	// Add PersistentLumpFiles to StartupFiles
	StartupFiles.push(...PersistentLumpFiles);
};

var GetVersionBases = async (version = '{{{ PACKAGE_VERSION }}}') => {
	// Build BASE dependency list
	// TODO Do MD5 checks on the remote _BASE files, in the
	// rare event that the base version should change.
	if (VersionBases instanceof Object && version in VersionBases)
		return VersionBases[version];
	let base = [version]; // initialize with our own version, so we can shift() from step 1.
	let baseVer = version;
	try {
		// On a brand-new setup, the store SHOULD be first created here.
		let customStore = new idbKeyval.Store('SRB2_DATA', 'FILES', 1);
		do {
			// Check IDB first
			childBaseVer = await idbKeyval.get(CompositeKey('_BASE', baseVer), customStore);
			if (childBaseVer) {
				if (!base.includes(childBaseVer)) {
					base.push(childBaseVer);
					baseVer = childBaseVer;
				} else
					break; // no circular references, thanks
			} else {
				// If IDB doesn't have the _BASE, then check the server
				childBaseVer = await fetch(`data/${baseVer}/_BASE`);
				if (!childBaseVer.ok)
					// assume no base version, but don't store anything
					// in case this was an error.
					break;
				else {
					childBaseVer = await childBaseVer.text();
					if (childBaseVer) {
						if (!base.includes(childBaseVer)) {
							console.log(`GetVersionBases: Logging Base ${childBaseVer} for version ${baseVer}`);
							await idbKeyval.set(CompositeKey('_BASE', baseVer), childBaseVer, customStore);
							base.push(childBaseVer);
							baseVer = childBaseVer;
						} else {
							await idbKeyval.set(CompositeKey('_BASE', baseVer), '', customStore);
							break; // no circular references, thanks
						}
					} else {
						await idbKeyval.set(CompositeKey('_BASE', baseVer), '', customStore);
						break; // no further bases
					}
				}
			}
		} while(baseVer);
	} catch (e) {
		// connection error, don't log anything or continue
		console.error(`GetVersionBases: ${version} (baseVer: ${baseVer}) - ${e}`);
		throw e;
	}
	if (!(VersionBases instanceof Object))
		VersionBases = {};
	VersionBases[version] = [...base];
	return base;
};

var InstallProgram = async (full = false, version = '{{{ PACKAGE_VERSION }}}', checkServer = false, progressCallback = null, finishedCallback = null) => {
	let bases = await GetVersionBases(version);
	await GetVersionFileLists(version);
	let files = [...InstallFiles]; // if re-implementing FullInstallFiles, do it here and check for `full`
	return CheckInstallFileList(files, version, checkServer, progressCallback, finishedCallback);
};

// UX Function
var UXInstallProgram = (full, version = '{{{ PACKAGE_VERSION }}}') => {
	HideContent();
	// TODO checkServer -- check for internet connection
	return InstallProgram(full, version, true, (name, curVal, maxVal) => {
		// Progress callback
		let percent = Math.round(Math.min(curVal, maxVal-1)/maxVal*100);
		// TODO: This is not accurate. In future, we can somehow reference the pool and check pool._pending
		// for more descriptive values
		if (typeof name === 'undefined') {
			console.log(`InstallProgram: Finished install (${curVal}/${maxVal}`);
			StatusElement.innerText = `Retrieving Data (${Math.min(maxVal-1)} of ${maxVal})...`;
		} else {
			console.log(`InstallProgram: Retrieving ${name} (${curVal}/${maxVal})`)
			//StatusElement.innerText = `Retrieving ${name.split('/').pop()}...`;
			StatusElement.innerText = `Retrieving Data (${Math.min(curVal, maxVal-1)} of ${maxVal})...`;
		}
		ProgressElement.value = percent;
	},
	() => {
		// Finished callback
		if (full) {
			alert('Finished install.');
			window.location.reload();
		} else {
			StatusElement.innerText = 'Loading...';
			// Currently, the ProgressElement is useless after this, so just hide
			ProgressElement.style.display = "none";
		}
	});
};

var RetrieveInstalledFile = async (fn, version = '{{{ PACKAGE_VERSION }}}', checkServer = false) => {
	// If IDB file exists, will return the IDB file without checking
	// the web server.
	let bases = await GetVersionBases(version);
	return CheckInstallProgramFile(fn, version, [...bases], [...bases], checkServer);
};

var WriteInstalledFileToFS = async (fn, version = '{{{ PACKAGE_VERSION }}}', checkServer = false) => {
	let idbFile = await RetrieveInstalledFile(fn, version, checkServer);
		if (idbFile instanceof Object && 'contents' in idbFile && idbFile['contents'])
			return WriteFS('/', fn, idbFile['contents']);
};

var DeleteInstalledFileFromFS = async (fn, persistentFilenames = []) => {
	if (!persistentFilenames.includes(fn))
		return DeleteFS(fn);
};

////////////////////////////////
// Addons
////////////////////////////////

var ExtractZipFile = (destBaseDir, buffer) => {
	return JSZip.loadAsync(buffer)
	.then((zip) => {
		let files = [];
		zip.forEach((path, file) => {
			if (!file['dir']) {
				files.push(
					zip.file(path).async('uint8array')
					.then((data) => {
						return WriteFS(destBaseDir, path, data);
					})
				);
			} else {
				try { FS.mkdir(`${destBaseDir}/${path}`); } catch(e) { }
			}
		});
		return Promise.all(files)
		.then(SyncFS)
		.catch((err) => console.log(err));
	});
};

var DownloadExtractZipFile = (url, destBaseDir, cors) => {
	let preUrl = cors ? 'https://cors-anywhere.herokuapp.com/' : '';
	return fetch(`${preUrl}${url}`, cors ? {mode: 'cors'} : {})
		.then((response) => response.blob())
		.then((buffer) => ExtractZipFile(destBaseDir, buffer))
		.catch((err) => console.log(err));
};

var DownloadDataFile = (url, destBaseDir, path, cors) => {
	let preUrl = cors ? 'https://cors-anywhere.herokuapp.com/' : '';
	return fetch(`${preUrl}${url}`, cors ? {mode: 'cors'} : {})
		.then((response) => response.arrayBuffer())
		.then((buffer) => WriteFS(destBaseDir, path, new Uint8Array(buffer)))
		.catch((err) => {
			console.log(err);
		});
};

var LoadAddons = () => {
	let addons = [];
	let inputs = document.getElementById("addonFilesContainer").querySelectorAll(".addonFilesField:not([id=addonFilesTemplate]) input[type=file]");
	inputs.forEach((input) => {
		if (input.files.length) {
			if (input.files[0].name.endsWith('.zip'))
				addons.push(ExtractZipFile('/addons', input.files[0]));
			else{
				addons.push(input.files[0].arrayBuffer()
				.then((buffer) => WriteFS('/addons', input.files[0].name.replace(' ', '_'), new Uint8Array(buffer)))
				.catch((err) => { console.log('LoadAddons():'); console.log(err); })
				);}
		}
	});
	return Promise.all(addons)
	.catch(err => { console.log('LoadAddons() error'); console.log(err); });
};

////////////////////////////////
// Emscripten Module -- Runtime Parameters
////////////////////////////////

var SystemArguments = [
	'+addons_option','CUSTOM',
	'+addons_folder','/addons',
];

var ControlArguments = [];

var UserArguments = [];

var BuildControlArguments = () => {
	let inputs = ControlsFormElement.querySelectorAll('input, textarea');
	let files = document.getElementById('addonFilesContainer').querySelectorAll('input[type=file]')
	let args = [];

	// do files first, so we can use the '-file' operator from SystemArguments
	let validFile = false;
	if (document.getElementById('addonStartup').checked) {
		files.forEach(input => {
			if (input.files.length && !input.files[0].name.endsWith('.zip')) {
				if (!validFile) {
					args.push('-file');
					validFile = true;
				}
				args.push(`/addons/${input.files[0].name.replace(' ', '_')}`);
			}
		});
	}

	inputs.forEach(input => {
		switch(input.id) {
			case 'resizeHeight': {
				let resizeHeight = input.value > 800 ? 0 : input.value;
				let dims = ResizeDimensions(
					GetViewportWidth(),
					GetViewportHeight(),
					resizeHeight
				);
				args.push(
					'+scr_resizeheight', resizeHeight.toString(),
					'-width', dims.x.toString(),
					'-height', dims.y.toString()
				);
				break;
			}
			case 'playMusic': {
				let val = (input.checked ? 'on' : 'off');
				args.push(
					'+midimusic',val,
					'+digimusic',val
				);
				break;
			}
			case 'playSound': {
				let val = (input.checked ? 'on' : 'off');
				args.push(
					'+sounds',val
				);
				break;
			}
			case 'useMouse': {
				if (!input.checked)
					args.push(
						'-nomouse'
					);
				break;
			}
			case 'drawDistance': {
				args.push('+drawdist',DrawDistanceRange[input.value]);
				break;
			}
			case 'shadow': {
				let val = (input.checked ? 'on' : 'off');
				args.push(
					'+shadow',val
				);
				break;
			}
			case 'midisoundfont': {
				if (input.checked) {
					args.push(
						'+midisoundfont','/florestan.sf2'
					);
				}
				break;
			}
		}
	});
	// Don't trust iPhone to give reliable resize events. Handle ourselves.
	if (UserAgentIsiPhone())
		args.push('+scr_resize', 'off');
	else
		args.push('+scr_resize', 'on');
	ControlArguments = args;
};

var BuildUserArguments = () => {
	let argString = document.getElementById('userArguments').value;
	let args = [];
	if (argString.trim().length > 0)
		args = argString.split(' ');
	UserArguments = args;
};

var SystemArgumentsToString = () => `${SystemArguments.join(' ')} ${ControlArguments.join(' ')}`;

////////////////////////////////
// Emscripten Module -- Runtime Parameters
////////////////////////////////

var StartedMainLoop = false;
var StartedMainLoopCallback = () => {
	document.getElementById('canvas').style.zIndex = "9999";
	document.getElementById('canvas').style.opacity = "1.0";
	document.body.classList.add('startedMainLoop');
	StartedMainLoop = true;
	// go scorched earth to save memory
	DeleteNode(document.getElementById('content'));
	// unlock mouse by default
	UnlockMouse(true);
};

var ErrorCrashed = false;

var ErrorHandler = (e, errorId) => {
	let index = Module['aliveId'].indexOf(errorId);
	if (index > -1) {
		if (!Module['alive'][index] && !ErrorCrashed) {
			ErrorCrashed = true;
			let msg = `PROGRAM ERROR\n\nIf you experience repeated crashes, try the Low-End mode under Settings > Advanced.\n\n${UserAgentIsMobile() ? "Tap" : "Click"} "${UserAgentIsiOS() ? "Close" : "OK"}" to restart the app.\n\nDEVELOPER INFO`;
			if (e && e.stack)
				msg += `\n\n${e.stack}`;
			else
				msg += `\n\n${e}`;
			msg += `\n\nSRB2 Version: ${PackageVersion}\n\nSRB2 Web Version: {{{ SHELL_VERSION }}}\n\n${navigator.userAgent}`
			console.error(msg);
			PauseLoop();
			try {
				if ('SDL2' in Module !== 'undefined'
					&& 'audioContext' in Module['SDL2']
					&& Module['SDL2'].audioContext)
					Module['SDL2'].audioContext.close();
			} catch(e) { }
			alert(msg);
			window.location.reload();
		}
		Module['alive'].splice(index, 1);
		Module['aliveId'].splice(index, 1);
	}
};

var InitiateErrorCheck = (e) => {
	if (!ErrorCrashed && e && typeof e === 'string' && e.includes('exception thrown:')) {
		let errorId = (+new Date + Math.random().toString(36).slice(-5));
		Module['alive'].push(false);
		Module['aliveId'].push(errorId);
		setTimeout(ErrorHandler, 1000/70, e, errorId); // NEWTICRATE*2
	}
};

var InvalidateErrorChecks = () => {
	Module['alive'].fill(true);
};

var GetJs = async (version = '{{{ PACKAGE_VERSION }}}') => {
	let customStore = new idbKeyval.Store('SRB2_DATA', 'FILES', 1);
	let js = await idbKeyval.get(CompositeKey('srb2.js', version), customStore);
	if (!(js instanceof Object) || !('contents' in js) || !js['contents']) {
		let msg = 'Runtime Error: Program JS not found!\n\nIf you see this error again, try resetting your program data under Settings > Storage.';
		console.log(msg);
		alert(msg);
		window.location.reload();
	} else {
		let string = new TextDecoder('utf-8').decode(js['contents']);
		return `data:text/javascript;base64,${btoa(string)}`;
	}
};

var GetWasm = async (version = '{{{ PACKAGE_VERSION }}}') => {
	let customStore = new idbKeyval.Store('SRB2_DATA', 'FILES', 1);
	let wasm = await idbKeyval.get(CompositeKey('srb2.wasm', version), customStore);
	if (!(wasm instanceof Object) || !('contents' in wasm) || !wasm['contents']) {
		let msg = 'Runtime Error: Program not found!\n\nIf you see this error again, try resetting your program data under Settings > Storage.';
		console.log(msg);
		alert(msg);
		window.location.reload();
	} else
		Module['wasmBinary'] = wasm['contents'];
};

var PackageVersion;

var Module = {
	arguments: [],

	calledRun: false,

	alive: [], // part of crash handler

	aliveId: [],

	wasmBinary: null,

	StripLeadingSeparators: StripLeadingSeparators,
	WriteInstalledFileToFS: WriteInstalledFileToFS,
	DeleteInstalledFileFromFS: DeleteInstalledFileFromFS,
	PersistentLumpFiles: PersistentLumpFiles,

	preRun: [() => {
		BuildControlArguments();
		BuildUserArguments();
		Module['arguments'].push(...SystemArguments, ...ControlArguments, ...UserArguments);

		////////////////////////////////
		// Mount filesystem, then check for music.dta
		////////////////////////////////

		addRunDependency('mount-filesystem');
		InitializeFS()
		.then(()=>{
			return Promise.all(StartupFiles.map(async fn => {
				try {
					await WriteInstalledFileToFS(fn, PackageVersion, false);
				} catch (e) {
					let msg = `Runtime Error: File ${fn} not found!\n\nIf you see this error again, try resetting your program data.`;
					console.error(msg);
					alert(msg);
					window.location.reload();
					return Promise.reject();
				}
			}));
		})
		.then(LoadAddons)
		.finally(() => removeRunDependency('mount-filesystem'));
	}],

	printErr: (e) => {
		// HACK to catch errors thrown by runIter
		console.error(e);
		InitiateErrorCheck(e);
	},

	quit: (e) => {
		InitiateErrorCheck(e);
	},

	onExit: () => {
		window.location.reload();
	},
	
	////////////////////////////////
	// Platform Stuff
	////////////////////////////////

	print: (function() {
		var element = document.getElementById('output');
		if (element) element.value = ''; // clear browser cache
		return function(text) {
			if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
			if (element) {
				element.value += text + "\n";
				element.scrollTop = element.scrollHeight; // focus on bottom
			}
		};
	})(),
	// printErr: function(text) {
	//	 if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
	//	 console.error(text);
	// },
	canvas: (function() {
		var canvas = document.getElementById('canvas');

		// As a default initial behavior, pop up an alert when webgl context is lost. To make your
		// application robust, you may want to override this behavior before shipping!
		// See http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
		canvas.addEventListener("webglcontextlost", function(e) { alert('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);

		return canvas;
	})(),
	setStatus: function(text) {
		if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
		if (text === Module.setStatus.last.text) return;
		var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
		var now = Date.now();
		if (m && now - Module.setStatus.last.time < 30) return; // if this is a progress update, skip it if too soon
		Module.setStatus.last.time = now;
		Module.setStatus.last.text = text;
		if (m) {
			text = m[1];
			ProgressElement.value = parseInt(m[2])*100;
			ProgressElement.max = parseInt(m[4])*100;
			ProgressElement.hidden = false;
		} else {
			text = "Starting...";
		}
		StatusElement.innerHTML = text;
	},
	totalDependencies: 0,
	monitorRunDependencies: function(left) {
		this.totalDependencies = Math.max(this.totalDependencies, left);
		Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
	}
};
Module.setStatus('Downloading...');
window.onerror = function(event) {
	// TODO: do not warn on ok events like simulating an infinite loop or exitStatus
	// Module.setStatus('Exception thrown, see JavaScript console');
	Module.setStatus = function(text) {
		if (text) Module.printErr('[post-exception status] ' + text);
	};
};

////////////////////////////////
// iOS Support
////////////////////////////////

// iOS Safari requires us to activate WebAudio context upon user touch.
// The runtime will use this same audioContext.
// https://paulbakaus.com/tutorials/html5/web-audio-on-ios/
var ActivateAudio = function() {
	if (typeof (Module['SDL2']) === 'undefined') {
			Module['SDL2'] = {};
	}
	var SDL2 = Module['SDL2'];
	if (typeof(Module['SDL2'].audioContext) !== 'undefined' 
			|| !Module['SDL2'].audioContext) {
			if (typeof (AudioContext) !== 'undefined') {
					SDL2.audioContext = new AudioContext();
			} else if (typeof (webkitAudioContext) !== 'undefined') {
					SDL2.audioContext = new webkitAudioContext();
			}
	}
	if (typeof(Module['SDL2'].audioContext) !== 'undefined' 
			&& SDL2.audioContext.currentTime == 0) {
		var buffer = SDL2.audioContext.createBuffer(1, 1, 44100);
		var source = SDL2.audioContext.createBufferSource();
		source.buffer = buffer;
		source.connect(SDL2.audioContext.destination);
		source.start();
	}

	if (Module['calledRun'])
		Module.ccall('COM_ImmedExecute',
			null,
			['string'],
			['restartaudio']
		);
};

var PreActivateAudio = () => {
	if (UserAgentIsiOS())
		window.addEventListener('touchend', ActivateAudio, {once: true});
	else
		ActivateAudio();
};

var HandleViewportChange = () => {
	ChangeResolution();
};

window.addEventListener('load', function() {
	if(UserAgentIsiOS()) {
		if(IsStandalone()) {
			PreActivateAudio();
			window.addEventListener('resize', HandleViewportChange, false);
		}
	}
}, {once: true});

////////////////////////////////
// Pause/Resume
////////////////////////////////

var SuspendAudioContext = () => {
	if ('SDL2' in Module 
			&& Module['SDL2'] instanceof Object 
			&& 'audioContext' in Module['SDL2'] 
			&& Module['SDL2'].audioContext)
		Module['SDL2'].audioContext.suspend();
};

var ResumeAudioContext = () => {
	if ('SDL2' in Module 
			&& Module['SDL2'] instanceof Object 
			&& 'audioContext' in Module['SDL2'] 
			&& Module['SDL2'].audioContext
			&& Module['SDL2'].audioContext.status !== 'closed')
		Module['SDL2'].audioContext.resume();
	else {
		if ('SDL2' in Module 
				&& Module['SDL2'] instanceof Object)
			Module['SDL2'].audioContext = null;
		PreActivateAudio();
	}
};

var PauseLoop = () => {
	if (Module['calledRun'])
		Module.ccall('pause_loop',
			'number',
			[],
			[]
		);
};

var ResumeLoop = () => {
	if (Module['calledRun'])
		Module.ccall('resume_loop',
			'number',
			[],
			[]
		);
};

var DoResume = () => {
	if (Module['calledRun']) {
		let SDL2 = null;
		if (typeof (Module['SDL2']) !== 'undefined')
			SDL2 = Module['SDL2'];

		if (typeof(SDL2.audioContext) === 'undefined'
				|| !SDL2
				|| !(SDL2 instanceof Object)
				|| !('audioContext' in SDL2)
				|| !SDL2.audioContext 
				|| SDL2.audioContext.status === 'closed') {
			SDL2.audioContext = null;
			PreActivateAudio();
		} else
			ResumeAudioContext();
		ResumeLoop();
		ChangeResolution();
	}
	window.removeEventListener('touchend', DoResume, {once: true});
	window.removeEventListener('click', DoResume, {once: true});
	window.removeEventListener('keydown', DoResume, {once: true});
};

var HandleVisibilityChange = (e) => {
	if (Module['calledRun']) {
		if (e.type === 'focus')
			DoResume();
		else {
			PauseLoop();
			SuspendAudioContext();
			// just in case the browser does not fire a 'focus'
			// event upon our return, allow the user to touch to wake
			window.addEventListener('touchend', DoResume, {once: true});
			window.addEventListener('click', DoResume, {once: true});
			window.addEventListener('keydown', DoResume, {once: true});
		}
	}
};

window.addEventListener('load', function() {
	// on iOS 12, only blur/focus fire on change to home or power button
	window.addEventListener('blur', HandleVisibilityChange);
	window.addEventListener('focus', HandleVisibilityChange);
}, {once: true});

////////////////////////////////
// On-Screen Keyboard
////////////////////////////////

var KeyCaptureElement = document.getElementById("keyCapture");
var TextCaptureElement = document.getElementById("textCapture");
var ActiveCaptureElement = null;
var CloseDelay = 0;

var I_RaiseScreenKeyboard = () => {
	CloseDelay = 0;
	// iOS users need to touch the keyCapture element to show the
	// keyboard, so don't close it on the next touch.
	if (UserAgentIsiOS())
		CloseDelay++;
	if (UserAgentIsAndroid()) {
		ActiveCaptureElement = TextCaptureElement;
		TextCaptureElement.parentElement.style.zIndex = '100000';
		TextCaptureElement.parentElement.style.opacity = '1.0';
		TextCaptureElement.disabled = false;
		TextCaptureElement.addEventListener('keydown', HandleKey, true);
		TextCaptureElement.addEventListener('keyup', HandleKey, true);
		TextCaptureElement.addEventListener('keypress', HandleKey, true);
	} else
		ActiveCaptureElement = KeyCaptureElement;
	KeyCaptureElement.value = ".";
	KeyCaptureElement.style.zIndex = '99999';
	KeyCaptureElement.addEventListener('touchend', HandleTouchKeyboard, false);
	ActiveCaptureElement.focus();
};

var I_KeyboardOnScreen = () => {
	return document.activeElement === ActiveCaptureElement;
};

var I_CloseScreenKeyboard = () => {
	ActiveCaptureElement.blur();
	KeyCaptureElement.style.zIndex = '-99999';
	TextCaptureElement.parentElement.style.zIndex = '-99999';
	TextCaptureElement.parentElement.style.opacity = '0.0';
	TextCaptureElement.disabled = true;
	KeyCaptureElement.removeEventListener('touchend', HandleTouchKeyboard, false);
	if (ActiveCaptureElement === TextCaptureElement) {
		TextCaptureElement.removeEventListener('keydown', CaptureKeyEvent, true);
		TextCaptureElement.removeEventListener('keyup', CaptureKeyEvent, true);
	}
};

var HandleTouchKeyboard = () => {
	if (CloseDelay-- <= 0)
		I_CloseScreenKeyboard();
};

var HandleKey = (e) => {
	// Android: Don't let any key events pass to the program
	e.stopPropagation();
};

var InjectText = () => {
	let text = ActiveCaptureElement.value;
	ActiveCaptureElement.focus();
	if (Module['calledRun'] && text) {
		Module.ccall('inject_text',
			null,
			['string'],
			[text]
		);
		// ENTER keydown
		Module.ccall('inject_keycode',
			null,
			['int','int'],
			[13,false]
		);
		// ENTER keyup 
		Module.ccall('inject_keycode',
			null,
			['int','int'],
			[13,true]
		);
	}
	ActiveCaptureElement.value = '';
};

////////////////////////////////
// Viewport Changes
////////////////////////////////

var ChangeResolution = (x, y) => {
	if (Module['calledRun']) {
		if (typeof x === 'undefined')
			x = GetViewportWidth();
		if (typeof y === 'undefined')
			y = GetViewportHeight();
		Module.ccall('change_resolution',
			'number',
			['number', 'number'],
			[x, y]
		);
	}
};

var AllowWindowResize = () => {
	return (!I_KeyboardOnScreen() || !UserAgentIsMobile() || 
		// If OSK and mobile, be careful about resizing.
		// We generally want to allow it. Portrait is safe because OSK's are not tall.
		//
		// However, landscape is dangerous because the keyboard may take most of the
		// screen, resulting in a viewport ratio that's way too wide,
		// resulting in a big window size that can crash the game. So do a sanity check.
		//
		// iPhone X	= 2.167 ratio
		GetViewportHeight() > GetViewportWidth() ||
		GetViewportWidth() / GetViewportHeight() <= 2.2);
};

var LockMouse = () => {
	if (StartedMainLoop) {
		CanvasElement.requestPointerLock();
		Module.ccall('lock_mouse', null, [], []);
	}
};

var UnlockMouse = (force = false) => {
	if (StartedMainLoop) {
		if (force && document.pointerLockElement)
			document.exitPointerLock(); // this method should fire again, so don't unlock_mouse right now
		else if (!document.pointerLockElement)
			Module.ccall('unlock_mouse', null, [], []);
	}
};

var GetViewportWidth = () => {
	// if (UserAgentIsAndroid()) {
	//	 if (document.fullscreenElement) // chrome android weirdness
	//		 return screen.width;
	//	 else
	//		 return window.innerWidth;
	// } else
		return document.documentElement.clientWidth;
};

var GetViewportHeight = () => {
	// if (UserAgentIsAndroid()) {
	//	 if (document.fullscreenElement) // chrome android weirdness
	//		 return screen.height;
	//	 else
	//		 return window.innerHeight;
	// } else
		return document.documentElement.clientHeight;
};

var CaptureFullscreenKey = (e) => {
	// Let F11 do fullscreen
	if (e instanceof KeyboardEvent && e.key === 'F11')
		e.stopPropagation();
};

window.addEventListener('mousedown', LockMouse, false);
document.addEventListener('pointerlockchange', _=>UnlockMouse(), false);
window.addEventListener('load', _=>{
	document.addEventListener('keydown', CaptureFullscreenKey, true);
	document.addEventListener('keyup', CaptureFullscreenKey, true);
	document.addEventListener('keypress', CaptureFullscreenKey, true);
}, {once:true});
