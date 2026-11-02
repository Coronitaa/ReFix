/* x86-only replacement for the MASM stub in steam_fwd.asm (x64 build keeps using the asm). */
void* SafeUnsupportedExportStub(void)
{
    return 0;
}
