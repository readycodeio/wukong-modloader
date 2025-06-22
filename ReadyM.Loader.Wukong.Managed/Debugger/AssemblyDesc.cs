namespace ReadyM.Loader.Wukong.Managed.Debugger;

[Serializable]
public readonly struct AssemblyDesc : IEquatable<AssemblyDesc>
{
    public readonly string asmFullName;

    public AssemblyDesc(string asmFullName)
    {
        this.asmFullName = asmFullName ?? throw new InvalidOperationException();
    }

    public bool Equals(AssemblyDesc other)
        => asmFullName == other.asmFullName;

    public override int GetHashCode()
    {
        return (asmFullName != null ? asmFullName.GetHashCode() : 0);
    }

    public override bool Equals(object obj)
        => obj is AssemblyDesc other && Equals(other);

    public static bool operator ==(AssemblyDesc x, AssemblyDesc y)
        => x.asmFullName == y.asmFullName;

    public static bool operator !=(AssemblyDesc x, AssemblyDesc y)
        => x.asmFullName != y.asmFullName;
}