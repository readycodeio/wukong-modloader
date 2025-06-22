using Mono.Cecil;
using Mono.Cecil.Cil;
using ReadyM.Loader.Wukong.Bootstrap;

namespace ReadyM.Loader.Wukong.Managed.Debugger;

internal class AssemblyCopyHelper
{
    private string? _tempName;
    private string? _tempPath;

    private string _reloadSuffix = "";

    private void EnsureTempName()
    {
        if (_tempPath == null)
        {
            var tempPath = Path.GetTempPath();
            _tempName = Path.GetRandomFileName();
            _tempName = _tempName.Replace(".", "_");
            _tempPath = Path.Combine(tempPath, _tempName);
            Directory.CreateDirectory(_tempPath);
        }
    }

    public string GetTempPath()
    {
        EnsureTempName();
        return _tempPath!;
    }

    public void SetReloadSuffix(string suffix)
    {
        _reloadSuffix = suffix;
    }

    public void MarkForReload(
        AssemblyRenameHelper renameHelper,
        string assemblyPath,
        out string asmName,
        out string renamedAsmName,
        out string asmFullName,
        out string renamedAsmFullName
    )
    {
        EnsureTempName();

        var asmDef = AssemblyDefinition.ReadAssembly(assemblyPath);
        var asmNameDef = asmDef.Name;
        asmFullName = asmNameDef.FullName;

        var renamedAsmNameDef = new AssemblyNameDefinition($"{asmDef.Name.Name}{_reloadSuffix}", asmDef.Name.Version);
        renamedAsmFullName = renamedAsmNameDef.FullName;
        var renamedAssemblyDesc = new AssemblyDesc(renamedAsmFullName);

        renameHelper.AddAssemblyRename(asmDef.Name.Name, renamedAsmNameDef.Name, renamedAssemblyDesc);

        asmName = asmNameDef.Name;
        renamedAsmName = renamedAsmNameDef.Name;
    }

    public static string GetTempFileName()
    {
        var result = Path.GetTempFileName();
        result = result.Replace(".", "_");
        return result;
    }

    public static string RealChangeExtension(string path, string newExt)
    {
        if (newExt.StartsWith("."))
            newExt = newExt.Substring(1);

        string newPath;
        if (path.EndsWith(".dll") || path.EndsWith(".pdb") || path.EndsWith(".exe"))
            newPath = path.Substring(0, path.Length - 4) + '.' + newExt;
        else
            newPath = path + "." + newExt;

        return newPath;
    }

    public void CreateAssemblyClone(
        string assemblyPath,
        string? subDir,
        out string copiedAssemblyPath,
        out string? copiedAssemblySymbols,
        AssemblyRenameHelper renameHelper,
        IAssemblyResolver resolver
    )
    {
        try
        {
            var assemblySymbols = RealChangeExtension(assemblyPath, "pdb");
            if (!File.Exists(assemblySymbols))
                assemblySymbols = null;

            copiedAssemblySymbols = null;
            var hasSymbols = assemblySymbols != null;

            var asmDef = AssemblyDefinition.ReadAssembly(assemblyPath, new ReaderParameters()
            {
                ReadingMode = ReadingMode.Deferred,
                AssemblyResolver = resolver,
            });

            EnsureTempName();

            var copyDir = _tempPath!;
            if (subDir != null)
            {
                copyDir = Path.Combine(_tempPath!, subDir);
                if (!Directory.Exists(copyDir))
                    Directory.CreateDirectory(copyDir);
            }

            var asmFullName = asmDef.FullName;
            var copiedAssemblyName = asmDef.Name.Name;
            if (renameHelper.HasAssemblyRename(asmDef.Name.Name))
            {
                var renamed = renameHelper.TranslateName(asmDef.Name.Name, new AssemblyDesc(asmFullName)).translated;
                var parsedRenamed = AssemblyNameReference.Parse(renamed.asmFullName);
                copiedAssemblyName = parsedRenamed.Name;
            }

            copiedAssemblyPath = Path.Combine(copyDir, RealChangeExtension(copiedAssemblyName, "dll"));

            // There's a bug in Mono.Cecil that prevents symbols from being read properly if there isn't a copy first.
            // So we first copy the original assembly into some random named location and read it from there
            var tempAssemblyPath = Path.Combine(copyDir, GetTempFileName());
            File.Copy(assemblyPath, tempAssemblyPath, true);

            var tempAssemblySymbols = default(string);
            if (hasSymbols)
            {
                tempAssemblySymbols = RealChangeExtension(tempAssemblyPath, "pdb");
                copiedAssemblySymbols = RealChangeExtension(copiedAssemblyPath, "pdb");

                File.Copy(assemblySymbols!, tempAssemblySymbols, true);
                File.Copy(assemblySymbols!, copiedAssemblySymbols, true);
            }

            asmDef.Dispose();

            Log.Debug($"Processing {assemblyPath}");
            asmDef = AssemblyDefinition.ReadAssembly(tempAssemblyPath, new ReaderParameters()
            {
                ReadSymbols = hasSymbols,
                SymbolReaderProvider = hasSymbols ? new PortablePdbReaderProvider() : null,
                AssemblyResolver = resolver,
            });

            var asmNameDef = new AssemblyNameDefinition(copiedAssemblyName, asmDef.Name.Version);
            asmDef.Name = asmNameDef;

            renameHelper.ProcessAssembly(asmDef);

            asmDef.Write(copiedAssemblyPath, new WriterParameters()
            {
                WriteSymbols = hasSymbols,
                SymbolWriterProvider = hasSymbols ? new PortablePdbWriterProvider() : null,
            });
            asmDef.Dispose();

            File.Delete(tempAssemblyPath);
            if (tempAssemblySymbols != null)
            {
                File.Delete(tempAssemblySymbols);
            }
        }
        catch (Exception ex)
        {
            Log.Error("Error creating assembly clone:");
            Log.Error(ex);
            throw;
        }
    }

    public void Clear()
    {
        _tempName = null;
        _tempPath = null;
    }
}