using Microsoft.Extensions.Logging;

namespace CSharpModBase;

public interface ICSharpModExV2 : ICSharpModEx
{
    bool IsDebug { get; }
    void SetLoggerFactory(ILoggerFactory loggerFactory);
    
    void LateInit();
}
