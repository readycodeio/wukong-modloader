namespace CSharpModBase.Input;

public class HotKeyData
{
    public HotKeyData(ModifierKeys modifiers, Key key)
    {
        Modifiers = modifiers;
        Key = key;
    }

    public HotKeyData() : this(ModifierKeys.None, Key.None) {}

    public ModifierKeys Modifiers { get; set; }
    public Key Key { get; set; }
    public GamePadButton GamePadButton { get; set; }

    public bool IsValid => Key != Key.None || GamePadButton != GamePadButton.None;
    public string KeyString => KeyUtils.KeyToString(Modifiers, Key);

    public static uint GetCode(ModifierKeys modifiers, Key key)
    {
        return ((uint)modifiers << 16) | (uint)key;
    }

    public uint Code
    {
        get
        {
            if (GamePadButton != GamePadButton.None)
            {
                return 0xF0000000 | (uint)GamePadButton;
            }
            else
            {
                return GetCode(Modifiers, Key);
            }
        }
    }

    public void SetFromCode(uint code)
    {
        if ((code >> 24) == 0xF0)
        {
            GamePadButton = (GamePadButton)(code & 0xFFFF);
            Modifiers = ModifierKeys.None;
            Key = Key.None;
        }
        else
        {
            GamePadButton = GamePadButton.None;
            Modifiers = (ModifierKeys)(code >> 16);
            Key = (Key)(code & 0xFF);
        }
    }

    public void SetKey(ModifierKeys modifiers, Key key)
    {
        Modifiers = modifiers;
        Key = key;
    }

    public override string ToString()
    {
        return KeyString;
    }
}