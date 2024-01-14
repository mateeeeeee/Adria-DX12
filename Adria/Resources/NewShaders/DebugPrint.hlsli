#ifndef _DEBUG_PRINT_
#define _DEBUG_PRINT_

#include "CommonResources.hlsli"

//https://therealmjp.github.io/posts/hlsl-printf/#setting-up-the-print-buffer

static const uint MaxDebugPrintArgs = 4;

template<typename T> uint CharToUint(in T c)
{
    if (c == ' ') return 32;
    if (c == '!') return 33;
    if (c == '"') return 34;
    if (c == '#') return 35;
    if (c == '$') return 36;
    if (c == '%') return 37;
    if (c == '&') return 38;
    if (c == '\'') return 39;
    if (c == '(') return 40;
    if (c == ')') return 41;
    if (c == '*') return 42;
    if (c == '+') return 43;
    if (c == ',') return 44;
    if (c == '-') return 45;
    if (c == '.') return 46;
    if (c == '/') return 47;
    if (c == '0') return 48;
    if (c == '1') return 49;
    if (c == '2') return 50;
    if (c == '3') return 51;
    if (c == '4') return 52;
    if (c == '5') return 53;
    if (c == '6') return 54;
    if (c == '7') return 55;
    if (c == '8') return 56;
    if (c == '9') return 57;
    if (c == ':') return 58;
    if (c == ';') return 59;
    if (c == '<') return 60;
    if (c == '=') return 61;
    if (c == '>') return 62;
    if (c == '?') return 63;
    if (c == '@') return 64;
    if (c == 'A') return 65;
    if (c == 'B') return 66;
    if (c == 'C') return 67;
    if (c == 'D') return 68;
    if (c == 'E') return 69;
    if (c == 'F') return 70;
    if (c == 'G') return 71;
    if (c == 'H') return 72;
    if (c == 'I') return 73;
    if (c == 'J') return 74;
    if (c == 'K') return 75;
    if (c == 'L') return 76;
    if (c == 'M') return 77;
    if (c == 'N') return 78;
    if (c == 'O') return 79;
    if (c == 'P') return 80;
    if (c == 'Q') return 81;
    if (c == 'R') return 82;
    if (c == 'S') return 83;
    if (c == 'T') return 84;
    if (c == 'U') return 85;
    if (c == 'V') return 86;
    if (c == 'W') return 87;
    if (c == 'X') return 88;
    if (c == 'Y') return 89;
    if (c == 'Z') return 90;
    if (c == '[') return 91;
    if (c == '\\') return 92;
    if (c == ']') return 93;
    if (c == '^') return 94;
    if (c == '_') return 95;
    if (c == '`') return 96;
    if (c == 'a') return 97;
    if (c == 'b') return 98;
    if (c == 'c') return 99;
    if (c == 'd') return 100;
    if (c == 'e') return 101;
    if (c == 'f') return 102;
    if (c == 'g') return 103;
    if (c == 'h') return 104;
    if (c == 'i') return 105;
    if (c == 'j') return 106;
    if (c == 'k') return 107;
    if (c == 'l') return 108;
    if (c == 'm') return 109;
    if (c == 'n') return 110;
    if (c == 'o') return 111;
    if (c == 'p') return 112;
    if (c == 'q') return 113;
    if (c == 'r') return 114;
    if (c == 's') return 115;
    if (c == 't') return 116;
    if (c == 'u') return 117;
    if (c == 'v') return 118;
    if (c == 'w') return 119;
    if (c == 'x') return 120;
    if (c == 'y') return 121;
    if (c == 'z') return 122;
    if (c == '{') return 123;
    if (c == '|') return 124;
    if (c == '}') return 125;
    if (c == '~') return 126;
    return 0;
}

template<typename T, uint N> uint StrLen(T str[N])
{
    return N;
}

enum ArgCode
{
    DebugPrint_Uint = 0,
    DebugPrint_Uint2,
    DebugPrint_Uint3,
    DebugPrint_Uint4,
    DebugPrint_Int,
    DebugPrint_Int2,
    DebugPrint_Int3,
    DebugPrint_Int4,
    DebugPrint_Float,
    DebugPrint_Float2,
    DebugPrint_Float3,
    DebugPrint_Float4,
    NumDebugPrintArgCodes
};

struct DebugPrintHeader
{
    uint NumBytes;
    uint StringSize;
    uint NumArgs;
};  

struct DebugPrinter
{
    static const uint BufferSize = 256;
    static const uint BufferSizeInBytes = BufferSize * sizeof(uint);
    uint InternalBuffer[BufferSize];
    uint ByteCount;
    uint StringSize;
    uint ArgCount;

    void Init()
    {
        for(uint i = 0; i < BufferSize; ++i) InternalBuffer[i] = 0;
        ByteCount = 0;
        StringSize = 0;
        ArgCount = 0;
    }

    uint CurrBufferIndex()
    {
        return ByteCount / 4;
    }

    uint CurrBufferShift()
    {
        return (ByteCount % 4) * 8;
    }

    void AppendChar(uint c)
    {
        if(ByteCount < BufferSizeInBytes)
        {
            InternalBuffer[CurrBufferIndex()] |= ((c & 0xFF) << CurrBufferShift());
            ByteCount += 1;
        }
    }

    template<typename T, uint N> void AppendArgWithCode(ArgCode code, T arg[N])
    {
        if(ByteCount + sizeof(arg) > BufferSizeInBytes) return;

        if(ArgCount >= MaxDebugPrintArgs) return;

        AppendChar(code);
        for(uint elem = 0; elem < N; ++elem)
        {
            for(uint b = 0; b < sizeof(T); ++b)
            {
                AppendChar(asuint(arg[elem]) >> (b * 8));
            }
        }
        ArgCount += 1;
    }
    
    void AppendArg(uint x)
    {
        uint a[1] = { x };
        AppendArgWithCode(DebugPrint_Uint, a);
    }

    void AppendArg(uint2 x)
    {
        uint a[2] = { x.x, x.y };
        AppendArgWithCode(DebugPrint_Uint2, a);
    }

    void AppendArg(uint3 x)
    {
        uint a[3] = { x.x, x.y, x.z };
        AppendArgWithCode(DebugPrint_Uint3, a);
    }

    void AppendArgs()
    {
    }

    template<typename T0> void AppendArgs(T0 arg0)
    {
        AppendArg(arg0);
    }

    template<typename T0, typename T1> void AppendArgs(T0 arg0, T1 arg1)
    {
        AppendArg(arg0);
        AppendArg(arg1);
    }

    template<typename T0, typename T1, typename T2> void AppendArgs(T0 arg0, T1 arg1, T2 arg2)
    {
        AppendArg(arg0);
        AppendArg(arg1);
        AppendArg(arg2);
    }

    template<typename T0, typename T1, typename T2, typename T3> void AppendArgs(T0 arg0, T1 arg1, T2 arg2, T3 arg3)
    {
        AppendArg(arg0);
        AppendArg(arg1);
        AppendArg(arg2);
        AppendArg(arg3);
    }

    void Commit()
    {
        if(ByteCount < 2) return;  
        ByteCount = ((ByteCount + 3) / 4) * 4;

        RWByteAddressBuffer printBuffer = ResourceDescriptorHeap[FrameCB.printfBufferIdx];
        const uint numBytesToWrite = ByteCount + sizeof(DebugPrintHeader);
        uint offset = 0;
        printBuffer.InterlockedAdd(0, numBytesToWrite, offset);
        offset += sizeof(uint);

        DebugPrintHeader header;
        header.NumBytes = ByteCount;
        header.StringSize = StringSize;
        header.NumArgs = ArgCount;

        printBuffer.Store<DebugPrintHeader>(offset, header);
        offset += sizeof(DebugPrintHeader);
        for(uint i = 0; i < ByteCount / 4; ++i)
            printBuffer.Store(offset + (i * sizeof(uint)), InternalBuffer[i]);
    }
};

#define DebugPrint(str, ...) do                              \
{                                                            \
    DebugPrinter printer;                                    \
    printer.Init();                                          \
    const uint strLen = StrLen(str);                         \
    for(uint i = 0; i < strLen; ++i)                         \
        printer.AppendChar(CharToUint(str[i]));              \
    printer.StringSize = printer.ByteCount;                  \
    printer.AppendArgs(__VA_ARGS__);                         \
    printer.Commit();                                        \
} while(0)





#endif