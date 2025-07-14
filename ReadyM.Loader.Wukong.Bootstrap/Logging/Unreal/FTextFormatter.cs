using System.Text.Json;
using System.Text.Json.Serialization;
using UnrealEngine.Runtime;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging.Unreal;

public class FTextConverter : JsonConverter<FText>
{
    public override FText Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        return FText.FromString(reader.GetString());
    }

    public override void Write(Utf8JsonWriter writer, FText value, JsonSerializerOptions options)
    {
        writer.WriteStringValue(value.ToString());
    }
}

public class FTextConverterFactory : JsonConverterFactory
{
    public override bool CanConvert(Type typeToConvert)
    {
        return typeToConvert == typeof(FText);
    }

    public override JsonConverter? CreateConverter(Type typeToConvert, JsonSerializerOptions options)
    {
        if (CanConvert(typeToConvert))
        {
            return new FTextConverter();
        }

        return null;
    }
}