using Microsoft.Extensions.Logging;
using Mono.Cecil;
using PreludeLib.CompileTime.Public;
using ReadyM.Loader.Wukong.Bootstrap.Registry;

namespace ReadyM.Loader.Wukong.Bootstrap.Preprocess;

public class AssemblyPreprocessor(PreprocessAssemblyResolver resolver, CompileTimePrelude prelude, ILogger logger)
{
    public void Preprocess(ModRegistry registry)
    {
        foreach (var dir in registry.ModDirs)
        {
            var modMeta = registry.MetaByDir[dir];
            if (modMeta.Disabled)
                return;

            logger.LogDebug("Preprocessing mod: {ModName}", modMeta.ModName);
            
            foreach (var modAsmPath in modMeta.AllAsmPaths)
            {
                var modAsmName = Path.GetFileNameWithoutExtension(modAsmPath);
                // NOTE: Thankfully the actual implementation ignores the version information, so we don't have to know
                // it beforehand.
                var modAsmNameRef = new AssemblyNameReference(modAsmName, new Version());
                using var modAsmDef = resolver.Resolve(modAsmNameRef);

                prelude.ScanAndPatchAll(modAsmDef);
            }
        }
        
        prelude.Commit();

        foreach (var patchedAsmDef in prelude.Backend.PatchedAssemblies)
        {
            logger.LogDebug("Marking assembly as dirty: {AsmName}", patchedAsmDef.Name.Name);
            resolver.SetDirty(patchedAsmDef);
        }
        
        resolver.Save();
    }
}