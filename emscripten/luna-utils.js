var Module;
// From emscripten: in order to reference the preexisting Module var or create it if needed.
if (!Module) Module = (typeof Module !== 'undefined' ? Module : null) || {};

Module['preRun'] = function() {

callLuna = function(inputFileName, tnsFileName)
{
    return (function MyCallMain(args) {
        args = args || [];
        ensureInitRuntime();
        var argc = args.length + 1;
        function pad() {
            for (var i = 0; i < 4 - 1; i++) {
                argv.push(0);
            }
        }
        var argv = [allocate(intArrayFromString(Module['thisProgram']), 'i8', ALLOC_NORMAL)];
        pad();
        for (var i = 0; i < argc - 1; i = i + 1) {
            argv.push(allocate(intArrayFromString(args[i]), 'i8', ALLOC_NORMAL));
            pad();
        }
        argv.push(0);
        argv = allocate(argv, 'i32', ALLOC_NORMAL);

        return Module['_main'](argc, argv, 0);
    })([inputFileName, tnsFileName]);
};

fileLoaded = function(event, filename)
{
    if (event.target.readyState == FileReader.DONE)
    {
        var tnsName = filename.replace(/\.(lua|xml)$/, '.tns');
        FS.writeFile(filename, new Uint8Array(event.target.result), {encoding: 'binary'});
        try { FS.unlink(tnsName); } catch (e){}
        var retVal = callLuna(filename, tnsName);
        if (retVal !== 0) {
            alert('Oops, something went wrong generating the .tns file (see console)');
            return;
        }
        var file = FS.readFile(tnsName, {encoding: 'binary'});
        if (file) {
            var blob = new Blob([file], {type: 'application/octet-stream'});
            window['saveAs'](blob, tnsName);
        } else {
            alert('Oops, something went wrong retrieving the generated .tns file :(');
        }
    }
};

fileLoad = function(event)
{
    var file = event.target.files[0];
    if (!file || (file.name.substr(-4) !== '.lua' && file.name.substr(-4) !== '.xml'))
    {
        alert('Error: a .lua script or .xml file is needed');
        return;
    }

    var reader = new FileReader();
    reader.onloadend = function(event) {
        fileLoaded(event, file.name);
    };
    reader.readAsArrayBuffer(file);
};

}; // preRun function

