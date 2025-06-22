namespace ReadyM.Loader.Wukong.Managed.Debugger;

public readonly struct Translated<T>
    where T : struct
{
    public readonly T original;
    public readonly T translated;

    public Translated(T original, T translated)
    {
        this.original = original;
        this.translated = translated;
    }
}
