// RealtimeTranslationMod: Windows DLL entry point (UE4SS loads this module).
//
// The actual mod logic lives in TranslationMod (see TranslationMod.cpp). This file exists to
// satisfy the DLL entry requirements on Windows.

#include <TranslationMod.hpp>

extern "C"
{
    __declspec(dllexport) RC::CppUserModBase* start_mod()
    {
        return new RC::RealtimeTranslation::TranslationMod();
    }

    __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
