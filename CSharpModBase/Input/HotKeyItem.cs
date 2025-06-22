using System;

namespace CSharpModBase.Input;

public class HotKeyItem : HotKeyData
{
    public string Label { get; set; }
    public Action Action { get; set; }
    public Action? EndAction { get; set; }
    /// <summary>
    /// 0为不支持连按，大于0为连按触发毫秒间隔
    /// </summary>
    public int RepeatMs { get; set; }
    /// <summary>
    /// 上一次触发的时间，毫秒
    /// </summary>
    public long LastTriggerMs { get; set; }
    /// <summary>
    /// 正在被按下
    /// </summary>
    public bool IsPressed { get; set; }
    public bool RunOnGameThread { get; set; } = true;

    public HotKeyItem WithKey(ModifierKeys Modifiers, Key Key)
    {
        return new(Label, Modifiers, Key, Action, EndAction);
    }

    public HotKeyItem(ModifierKeys modifiers, Key key, Action action, Action? endAction = null) : this("", modifiers, key, action, endAction)
    {
    }

    public HotKeyItem(string label, ModifierKeys modifiers, Key key, Action action, Action? endAction = null) : base(modifiers, key)
    {
        Label = label;
        Action = action;
        EndAction = endAction;
    }

    public HotKeyItem() : this("", ModifierKeys.None, Key.None, EmptyAction)
    {
    }

    private static void EmptyAction()
    {
    }
}
