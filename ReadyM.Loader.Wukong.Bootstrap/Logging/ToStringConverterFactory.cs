using System.Text.Json;
using System.Text.Json.Serialization;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

public class ToStringConverterFactory : JsonConverterFactory
{
    public override bool CanConvert(Type typeToConvert)
        => true;

    public override JsonConverter CreateConverter(Type typeToConvert, JsonSerializerOptions options)
    {
        var wrapperType = typeof(ToStringConverter<>).MakeGenericType(typeToConvert);
        return (JsonConverter)Activator.CreateInstance(wrapperType)!;
    }
}