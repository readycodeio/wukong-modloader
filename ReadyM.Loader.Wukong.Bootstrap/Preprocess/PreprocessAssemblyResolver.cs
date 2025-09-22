using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;
using Microsoft.Extensions.Logging;
using Mono.Cecil;
using ReadyM.Loader.Wukong.Bootstrap.Registry;
using ReadyM.Loader.Wukong.Bootstrap.Settings;

namespace ReadyM.Loader.Wukong.Bootstrap.Preprocess;

public unsafe class PreprocessAssemblyResolver : IAssemblyResolver
{
    private struct Entry
    {
        public AssemblyDefinition? AssemblyDef;

        public string? Location;
        public MonoBundledAssembly* BundledItemPtr;
 
        public bool IsDirty;
        public bool IsBundled => BundledItemPtr != null;
    }

    private readonly DefaultAssemblyResolver _fallbackResolver = new();

    private readonly GlibAllocator _allocator;
    private readonly List<Entry> _entries = [];
    private readonly Dictionary<string, int> _entryByName = [];
    private readonly ILogger _logger;

    public PreprocessAssemblyResolver(MonoBundledAssemblyArray array, GlibAllocator allocator, PathSettings pathSettings, ModRegistry modRegistry, ILogger logger)
    {
        _logger = logger;
        _allocator = allocator;
        
        for (var itemPtr = array.FirstItemPtr; *itemPtr != null; itemPtr++)
        {
            var item = *itemPtr;
            
            var name = Marshal.PtrToStringAnsi(new IntPtr(item->name));
            if (name == null)
                continue;
            if (_entryByName.ContainsKey(name))
                continue;

            logger.LogDebug("Found bundled assembly at {Ptr}", name);

            var entryIndex = _entries.Count;
            var entry = new Entry()
            {
                AssemblyDef = null,
                BundledItemPtr = item,
                Location = null,
                IsDirty = false,
            };
            _entries.Add(entry);
            _entryByName.Add(name, entryIndex);
        }
        
        _fallbackResolver.AddSearchDirectory(pathSettings.LoaderDir);
        _fallbackResolver.AddSearchDirectory(Path.Combine(pathSettings.LoaderDir, "Overrides"));
        _fallbackResolver.AddSearchDirectory(Path.Combine(pathSettings.ModDir, "Common"));
        _fallbackResolver.AddSearchDirectory(Path.Combine(pathSettings.ModDir, "Overrides"));
        
        foreach (var dir in modRegistry.ModDirs)
        {
            var modMeta = modRegistry.MetaByDir[dir];
            if (modMeta.Disabled)
                continue;
            
            _fallbackResolver.AddSearchDirectory(dir);
            foreach (var recurDir in Directory.EnumerateDirectories(dir, "*", SearchOption.AllDirectories))
            {
                _fallbackResolver.AddSearchDirectory(recurDir);
            }
        }
    }
    
    private void AddEntry(AssemblyDefinition asmDef)
    {
        Entry entry;
        if (_entryByName.TryGetValue(asmDef.Name.Name, out var entryIndex))
        {
            entry = _entries[entryIndex];
            entry.AssemblyDef = asmDef;
            _entries[entryIndex] = entry;
        }
        else
        {
            entryIndex = _entries.Count;
            entry = new Entry()
            {
                Location = asmDef.MainModule.FileName,
                AssemblyDef = asmDef,
                BundledItemPtr = null!,
            };
            _entries.Add(entry);
            _entryByName.Add(asmDef.Name.Name, entryIndex);
        }
    }

    private bool TryGetBundled(string name, [NotNullWhen(true)] out AssemblyDefinition? asmDef)
    {
        if (!_entryByName.TryGetValue(name, out var entryIndex))
        {
            asmDef = null;
            return false;
        }

        var entry = _entries[entryIndex];
        if (entry.AssemblyDef == null)
        {
            var size = entry.BundledItemPtr->size;
            var data = entry.BundledItemPtr->data;
            var stream = new UnmanagedMemoryStream(data, size);
            entry.AssemblyDef = AssemblyDefinition.ReadAssembly(stream, new ReaderParameters()
            {
                ReadWrite = true,
                AssemblyResolver = this,
            });
            _entries[entryIndex] = entry;
        }
        
        asmDef = entry.AssemblyDef;
        return true;
    }

    public AssemblyDefinition Resolve(AssemblyNameReference name)
        => Resolve(name, new ReaderParameters()
        {
            ReadWrite = true,
            AssemblyResolver = this,
        });

    public AssemblyDefinition Resolve(AssemblyNameReference name, ReaderParameters parameters)
    {
        _logger.LogDebug("Resolving assembly {Name}", name.Name);
        if (TryGetBundled(name.Name, out var asmDef))
        {
            _logger.LogDebug("Resolved assembly {Name} from bundled", name.Name);
            return asmDef;
        }
        
        asmDef = _fallbackResolver.Resolve(name, parameters);
        AddEntry(asmDef);
        return asmDef!;
    }

    public void Save()
    {
        // NOTE: The allocation size corresponds to the largest assemblies
        using var stream = new MemoryStream(48_000_000);
        
        for (var i = 0; i < _entries.Count; i++)
        {
            var entry = _entries[i];

            if (!entry.IsDirty)
                continue;

            Debug.Assert(entry.AssemblyDef != null);

            entry.IsDirty = false;
            if (entry.IsBundled)
            {
                stream.Position = 0;
                entry.AssemblyDef!.Write(stream);
                var newSize = (int)stream.Position;
                var newData = _allocator.Alloc(newSize);
                Marshal.Copy(stream.GetBuffer(), 0, newData, newSize);

                // NOTE: Yes, we're leaking memory here. We don't know the allocator that was used to allocate the original data.
                // Potentially, this data might have been statically compiled in. This isn't much memory so this is a non-issue.
                entry.BundledItemPtr->data = (byte*)newData;
                entry.BundledItemPtr->size = (uint)newSize;
            }
            else
            {
                Debug.Assert(entry.Location != null);
                entry.AssemblyDef!.Write();
            }
            
            _entries[i] = entry;
        }
    }

    public void Dispose()
    {
        _fallbackResolver.Dispose();
        _entries.Clear();
        _entryByName.Clear();
    }
}