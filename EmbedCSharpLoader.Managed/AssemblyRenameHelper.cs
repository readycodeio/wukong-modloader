using Mono.Cecil;

namespace EmbedCSharpLoader.Managed;

internal class AssemblyRenameHelper
{
    private struct Entry
    {
        public string asmName;
        public AssemblyDesc desc;
    }

    private readonly Dictionary<string, Entry> _assemblyNameTranslations =
        new Dictionary<string, Entry>();

    public void AddAssemblyRename(string oldName, string newName, AssemblyDesc newDesc)
    {
        _assemblyNameTranslations[oldName] = new Entry()
        {
            desc = newDesc,
            asmName = newName
        };
    }

    public bool HasAssemblyRename(string asmName)
    {
        if (!_assemblyNameTranslations.TryGetValue(asmName, out var entry))
            return false;

        return entry.asmName != asmName;
    }

    public Translated<AssemblyDesc> TranslateName(string oldName, AssemblyDesc oldDesc)
    {
        var origDesc = oldDesc;

        var asmDesc = oldDesc;
        var asmName = oldName;

        while (true)
        {
            if (!_assemblyNameTranslations.TryGetValue(oldName, out var resultEntry))
                break;
            if (resultEntry.asmName == asmName)
                break;

            asmDesc = resultEntry.desc;
            asmName = resultEntry.asmName;
        }

        var translatedAsmDesc = new Translated<AssemblyDesc>(origDesc, asmDesc);
        return translatedAsmDesc;
    }

    private AssemblyNameReference TranslateName(AssemblyNameReference asmRef)
    {
        if (!_assemblyNameTranslations.ContainsKey(asmRef.Name))
            return asmRef;

        var asmDesc = new AssemblyDesc(asmRef.FullName);
        asmDesc = TranslateName(asmRef.Name, asmDesc).translated;
        asmRef = AssemblyNameReference.Parse(asmDesc.asmFullName);
        return asmRef;
    }

    public void ProcessAssembly(AssemblyDefinition asmDef)
    {
        for (var i = 0; i < asmDef.Modules.Count; i++)
        {
            var module = asmDef.Modules[i];
            for (var j = 0; j < module.AssemblyReferences.Count; j++)
            {
                var asmRef = module.AssemblyReferences[j];
                asmRef = TranslateName(asmRef);
                module.AssemblyReferences[j] = asmRef;
            }
        }
    }

    public void Clear()
    {
        _assemblyNameTranslations.Clear();
    }
}
