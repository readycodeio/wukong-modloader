using Microsoft.Extensions.Logging;
using Mono.Cecil;
using PreludeLib.CompileTime.Public;
using PreludeLib.Tests.Preprocess;

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
            
            var mainAsmName = Path.GetFileNameWithoutExtension(modMeta.MainAsmPath);
            // NOTE: Thankfully the actual implementation ignores the version information, so we don't have to know
            // it beforehand.
            var mainAsmNameRef = new AssemblyNameReference(mainAsmName, new Version());
            var mainAsmDef = resolver.Resolve(mainAsmNameRef);

            prelude.ScanAndPatchAll(mainAsmDef);
        }
        
        resolver.Save();
    }
}