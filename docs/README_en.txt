* glTF-Maya-Exporter
　Kashika Inc.

** Installation
　You can choose either A or B to install this plugin

　A: Install Maya system folder

　　Copy "glTFExporter.mll" into "C:\Program Files\Autodesk\Maya2018\bin\plug-ins"
    Copy "glTFExporterOptions.mel" into "C:\Program Files\Autodesk\Maya2018\scripts\others"
    Copy "glTFExporterOptions.res.mel" into "C:\Program Files\Autodesk\Maya2018\scripts\others"

　B: Install your user folder
    Make folder at "C:\Users\[your-account]\Documents\maya\2018\plug-ins"
　　Copy "glTFExporter.mll" into "C:\Program Files\Autodesk\Maya2018\bin\plug-ins"
    Copy "glTFExporterOptions.mel" into "C:\Users\[your-account]\Documents\maya\2018\en_US\scripts"
    Copy "glTFExporterOptions.res.mel" into "C:\Users\[your-account]\Documents\maya\2018\en_US\scripts"
    Execute the below commnad on the Maya to set environment variable
	putenv "MAYA_PLUG_IN_PATH" "C:\Users\[your-account]\Documents\maya\2018\plug-ins"

** Options
　- Recalc normals: This plugin recalculates mesh normals when this flag is "On".
　- Output glb file: choose output format gltf or glb.
　- Compress buffer: Bin: glTF default type buffer, Draco: compress buffer using by draco.
　- Convert texture format: N/A:not convert texture format, Jpeg: Convert image textures to jpeg format, Png: Convert image textures to png format.
