using System.Text.Json;
using System.Text.Json.Serialization;

namespace ReadyM.Loader.Wukong.Bootstrap.Logging;

public class ToStringConverter<T> : JsonConverter<T>
{
    public override T? Read(
        ref Utf8JsonReader reader,
        Type typeToConvert,
        JsonSerializerOptions options)
    {
        throw new NotSupportedException();
    }

    public override void Write(Utf8JsonWriter writer, T value, JsonSerializerOptions options)
    {
        writer.WriteStringValue(value?.ToString() ?? "null");
    }
}