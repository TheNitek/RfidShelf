#include "ShelfHtml.h"

const char ShelfHtml::INDEX[] PROGMEM = 
R"=====(
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html><head><meta charset="utf-8"><script>
function _(e){return document.getElementById(e);}
function $(q){return Array.from(document.querySelectorAll(q))}
function b(c){ document.body.innerHTML=c}
function t(c,v){$(c).forEach(function(e){e.innerText = v})}
function ajax(m, url, cb, p) {var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState == 4) { cb(xhr.responseText); }}; xhr.open(m, url, true); xhr.send(p);}
function deleteUrl(url){ if(confirm("Delete?"))ajax('DELETE', url, reload);}
function upload(folder){ var fileInput = _('fileInput'); if(fileInput.files.length === 0){ alert('Choose a file first'); return; } var fileTooLong = false; Array.prototype.forEach.call(fileInput.files, function(file) { if (file.name.length > 100) { fileTooLong = true; }}); if (fileTooLong) { alert("File name too long. Files can be max. 100 characters long."); return; } xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 4) { location.reload(); }};
var d = new FormData(); for(var i = 0; i < fileInput.files.length; i++) { d.append('data', fileInput.files[i], folder.concat(fileInput.files[i].name)); }; xhr.open('POST', '/');
xhr.upload.addEventListener('progress', progressHandler, false); xhr.addEventListener('load', completeHandler, false); xhr.addEventListener('error', errorHandler, false); xhr.addEventListener('abort', abortHandler, false); _('ulDiv').style.display = 'block'; xhr.send(d); }
function progressHandler(event) { var percentage = Math.round((event.loaded / event.total) * 100); _('progressBar').value = percentage; _('ulStatus').innerHTML = percentage + '% (' + Math.ceil((event.loaded/(1024*1024)) * 10)/10 + '/' + Math.ceil((event.total/(1024*1024)) * 10)/10 + 'MB) uploaded'; }
function completeHandler(event) { _('ulStatus').innerHTML = event.target.responseText; location.reload(); }
function errorHandler(event) { _('ulStatus').innerHTML = 'Upload Failed'; }
function abortHandler(event) { _('ulStatus').innerHTML = 'Upload Aborted'; }
function reload(){ location.reload(); }
function loadStatus(){ajax('GET', '/?status=1', renderStatus)}
function loadFS(url){ajax('GET', url+'?fs=1', renderFS)}
function writeRfid(url){ if(url.length > 16) {alert('Folder name cannot have more than 16 characters'); return;} var d = new FormData(); d.append('write', 1); ajax('POST', url, renderStatus, d);}
function play(url){var d = new FormData(); d.append('play', 1); ajax('POST', url, renderStatus, d);}
function playFile(url){var d = new FormData(); d.append('playfile', 1); ajax('POST', url, renderStatus, d);}
function rootAction(action){var d = new FormData(); d.append(action, 1); ajax('POST', '/', renderStatus, d);}
function volume(action){var d = new FormData(); d.append(action, 1); ajax('POST', '/', renderStatus, d);}
function mkdir(){var f = _('folder').value; if(f != ''){var d = new FormData(); d.append('newFolder', f); ajax('POST', '/', reload, d);}}
function ota(){var d = new FormData(); d.append('ota', 1); ajax('POST', '/', function(){ b('Please wait and do NOT turn off the power!'); location.reload();}, d);}
function downloadpatch(){ var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (xhr.readyState === 1) { document.write('Please wait while downloading patch! When the download was successful the system is automatically restarting.'); } else if (xhr.readyState === 4) { location.reload(); }}; xhr.open('POST', '/'); var formData = new FormData(); formData.append('downloadpatch', 1); xhr.send(formData);}
function formatBytes(a){if(0==a)return"0 Bytes";var c=1024,e=["Bytes","KB","MB","GB"],f=Math.floor(Math.log(a)/Math.log(c));return parseFloat((a/Math.pow(c,f)).toFixed(2))+" "+e[f]}
function formatNumbers(){ $('.number').forEach(function(n){n.innerText = formatBytes(n.innerText); })}
function renderFS(r){
 var p = JSON.parse(r);
 _('fs').innerHTML = '';
 _('uploadForm').innerHTML=_('uploadForm').innerHTML.replace(/\{path\}/g, p.path);
 if(p.path == '/'){$('.hiddenNoRoot').forEach(function(e){e.style.display='initial'}); $('.hiddenRoot').forEach(function(e){e.style.display='none'});}
 else{$('.hiddenRoot').forEach(function(e){e.style.display='initial'}); $('.hiddenNoRoot').forEach(function(e){e.style.display='none'});}
 p.fs.sort(sortFS); p.fs.forEach(renderFSentry); formatNumbers();
 $('.file').forEach(function(e){e.innerHTML=e.innerHTML.replace(/\{path\}/g, p.path)})}
function sortFS(a, b){ if(('size' in a) == ('size' in b)) return ('' + a.name).localeCompare(b.name); else if(!('size' in a)) return -1; else return 1;}
function renderFSentry(e){
 if('size' in e){ let t = document.importNode(_('fileT').content.querySelector('div'), true); t.innerHTML = t.innerHTML.replace(/\{filename\}/g, e.name).replace(/\{filesize\}/g, e.size); if(e.name.endsWith('.mp3')){t.classList.add('mp3')} _('fs').appendChild(t);}
 else{ let t = document.importNode(_('folderT').content.querySelector('div'), true); t.innerHTML = t.innerHTML.replace(/\{foldername\}/g, e.name); _('fs').appendChild(t);}
}
function renderStatus(r){
 var s = JSON.parse(r);
 if('pairing' in s){t('.pairingFile', '/' + s.pairing); _('pairing').style.display='initial'}
 else{_('pairing').style.display='none'}
 var pb=_('playback');
 if('currentFile' in s){t('.currentFile', s.currentFile)}
 if(s.playback == 'FILE'){pb.className='playing'}
 else if(s.playback == 'PAUSED'){pb.className='paused'}
 else{pb.className='hidden'}
 var v=_('volume');
 if(s.night){v.className='night'}
 else{v.className='noNight'}
 _('volumeBar').value=s.volume; _('volumeBar').innerHTML=s.volume;
 if(!s.patch){_('patchForm').style.display='initial'}
 t('.firmwareVersion', s.version);
 t('.shelfTime', (new Date(s.time * 1000)).toUTCString());
}
</script>
<link rel="icon" href="data:;base64,iVBORw0KGgo=">
<title>RfidShelf</title>
<style>
 body { font-family: Arial, Helvetica;}
 #fs {vertical-align: middle;} #fs div:nth-child(even) { background: LightGray;}
 a { color: #0000EE; text-decoration: none;}
 a.del { color: red;}
 .mp3only, .hiddenPlaying, .hiddenPaused, .hidden, .hiddenNight, .hiddenNoNight, .hiddenNoRoot, .hiddenRoot { display: none;}
 .mp3 .mp3only, .playing .hiddenPaused, .paused .hiddenPlaying, .night .hiddenNoNight, .noNight .hiddenNight { display: initial;}
</style>
</head><body>
<template id="folderT">
 <div class="folder">&#x1f4c2; <a href="#/" onclick="loadFS('{foldername}'); return false;">{foldername}</a> 
 <a href="#" class="del" onclick="deleteUrl('{foldername}'); return false;" title="delete">&#x2718;</a> 
 <a href="#" onclick="writeRfid('{foldername}');" title="write to card">&#x1f4be;</a> 
 <a href="#" onclick="play('{foldername}'); return false;" title="play folder">&#x25b6;</a></div>
</template>
<template id="fileT">
 <div class="file">
 <span class="mp3only">&#x266b; </span>
 <a href="{path}/{filename}">{filename}</a> (<span class="number">{filesize}</span>) 
 <a href="#" class="del" onclick="deleteUrl('{path}/{filename}'); return false;" title="delete">&#x2718;</a> 
 <a href="#" class="mp3only" onclick="playFile('{path}/{filename}'); return false;" title="play">&#x25b6;</a></div>
</template>
<p style="font-weight: bold; display: none;" id="pairing">Pairing mode active. Place card on shelf to write current configuration for <span class="pairingFile" style="color:red"> </span> onto it</p>
<p id="playback" class="hidden">Currently playing: <strong class="currentFile"> </strong><br>
 <a class="hiddenPlaying" href="#" onclick="rootAction('resume'); return false;">&#x25b6;</a>
 <a class="hiddenPaused" href="#" onclick="rootAction('pause'); return false;">&#x23f8;</a>
 <a href="#" onclick="rootAction('stop'); return false;">&#x23f9;</a>
 <a href="#" onclick="rootAction('skip'); return false;">&#x23ed;</a></p>
<p id="volume">
 Volume:&nbsp;<meter id="volumeBar" value="0" max="50" style="width:300px;">0</meter><br>
 <a title="decrease volume" href="#" onclick="volume('volumeDown'); return false;">&#x1f509;</a> / 
 <a title="increase volume" href="#" onclick="volume('volumeUp'); return false;">&#x1f50a;</a> 
 <a class="hiddenNoNight" href="#" title="deactivate night mode" onclick="rootAction('toggleNight'); return false;">&#x1f31b;</a>
 <a class="hiddenNight" href="#" title="activate night mode" onclick="rootAction('toggleNight'); return false;">&#x1f31e;</a>
</p>
<form class="hiddenNoRoot" onsubmit="mkdir(); return false;">Create new folder: 
 <input type="text" name="folder" id="folder">
 <input type="button" value="Create" onclick="mkdir(); return false;"></form>
<form class="hiddenRoot" id="uploadForm"><p>Upload MP3 file: <input type="file" multiple="true" name="data" accept=".mp3" id="fileInput">
 <input type="button" value="upload" onclick="upload('{path}'); return false;"></p>
 <div id="ulDiv" style="display:none;"><progress id="progressBar" value="0" max="100" style="width:300px;"></progress>
 <p id="ulStatus"></p></div></form>
<p class="hiddenRoot"><a href="#" onclick="loadFS('/'); return false;">Back to top</a></p>
<div id="fs">Loading ...</div>
<form id="patchForm" class="hidden"><p><b>MP3 decoder patch missing</b> (might reduce sound quality) <input type="button" value="Download + Install VS1053 patch" onclick="downloadpatch(); return false;"></p></form>
<form id="firmwareForm"><p>Version <span class="firmwareVersion"> </span> <input type="button" value="Update Firmware" onclick="ota(); return false;"></p></form>
<p id="time">Shelf Time: <span class="shelfTime"> </span></p>
<script>loadStatus(); loadFS('.')</script>
</body></html>
)====="
;