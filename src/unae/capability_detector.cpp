// =============================================================================
// ReFix - Universal Network Arbitration Engine (UNAE)
// capability_detector.cpp - Implementation
// =============================================================================
#include "capability_detector.h"
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <vector>

extern void ReFixLog(const char* fmt, ...);

namespace UNAE {

CapabilityDetector& CapabilityDetector::Instance() {
    static CapabilityDetector s_instance;
    return s_instance;
}

std::string CapabilityDetector::GetExecutableDirectory() const {
    char path[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string s(path);
    size_t pos = s.find_last_of("\\/");
    return (pos != std::string::npos) ? s.substr(0, pos + 1) : ".\\";
}

std::string CapabilityDetector::GetExecutableBaseName() const {
    char path[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string s(path);
    size_t posSlash = s.find_last_of("\\/");
    std::string filename = (posSlash != std::string::npos) ? s.substr(posSlash + 1) : s;
    size_t posDot = filename.find_last_of('.');
    return (posDot != std::string::npos) ? filename.substr(0, posDot) : filename;
}

static bool StrContainsIgnoreCase(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    for (; *haystack; ++haystack) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && (tolower((unsigned char)*h) == tolower((unsigned char)*n))) {
            ++h;
            ++n;
        }
        if (!*n) return true;
    }
    return false;
}

static bool FileExists(const std::string& fullPath) {
    DWORD dwAttrib = GetFileAttributesA(fullPath.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

// -----------------------------------------------------------------------------
// Phase 1: PE Headers & Import Address Table (IAT) - Pure C-style SEH
// -----------------------------------------------------------------------------
static bool ScanIATSafe(HMODULE hExe, bool* pWinsock, bool* pNano, bool* pSteam, bool* pEOS) {
    __try {
        const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(hExe);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

        const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(
            reinterpret_cast<const uint8_t*>(hExe) + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

        const IMAGE_DATA_DIRECTORY& importDir =
            ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDir.VirtualAddress == 0 || importDir.Size == 0) return false;

        const auto* importDesc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(
            reinterpret_cast<const uint8_t*>(hExe) + importDir.VirtualAddress);

        while (importDesc->Name != 0) {
            const char* modName = reinterpret_cast<const char*>(
                reinterpret_cast<const uint8_t*>(hExe) + importDesc->Name);
            if (modName) {
                if (_stricmp(modName, "ws2_32.dll") == 0)           *pWinsock = true;
                else if (_stricmp(modName, "nanosockets.dll") == 0) *pNano = true;
                else if (_stricmp(modName, "steam_api64.dll") == 0) *pSteam = true;
                else if (StrContainsIgnoreCase(modName, "EOSSDK"))   *pEOS = true;
            }
            importDesc++;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void CapabilityDetector::ScanPhase1_IAT() {
    HMODULE hExe = GetModuleHandleA(NULL);
    if (!hExe) return;

    if (!ScanIATSafe(hExe, &m_caps.hasWinsock, &m_caps.hasNanosockets, &m_caps.hasSteamApi, &m_caps.hasEOS)) {
        ReFixLog("[UNAE:Detector] Warning: Exception occurred during PE/IAT scan");
    }
}

// -----------------------------------------------------------------------------
// Phase 2: Loaded Modules & Runtimes
// -----------------------------------------------------------------------------
void CapabilityDetector::ScanPhase2_LoadedModules() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (hSnap == INVALID_HANDLE_VALUE) return;

    MODULEENTRY32 me{};
    me.dwSize = sizeof(me);
    if (Module32First(hSnap, &me)) {
        do {
            std::string modName = me.szModule;
            std::transform(modName.begin(), modName.end(), modName.begin(), ::tolower);

            if (modName == "nanosockets.dll") m_caps.hasNanosockets = true;
            if (modName.find("eossdk") != std::string::npos) m_caps.hasEOS = true;
            if (modName == "steam_api64.dll") m_caps.hasSteamApi = true;
            if (modName == "unityplayer.dll" || modName == "mono-2.0-bdwgc.dll") {
                m_caps.engineName = "Unity";
            }
        } while (Module32Next(hSnap, &me));
    }
    CloseHandle(hSnap);
}

// -----------------------------------------------------------------------------
// Phase 3: Unity Managed Assemblies
// -----------------------------------------------------------------------------
void CapabilityDetector::ScanPhase3_UnityManaged() {
    std::string exeDir = GetExecutableDirectory();
    std::string exeBase = GetExecutableBaseName();

    // Check standard <Game>_Data/Managed/ directory
    std::vector<std::string> candidateManagedDirs = {
        exeDir + exeBase + "_Data\\Managed\\",
        exeDir + "Data\\Managed\\"
    };

    // If not found, search first matching *_Data directory
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA((exeDir + "*_Data").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            candidateManagedDirs.push_back(exeDir + fd.cFileName + "\\Managed\\");
        }
        FindClose(hFind);
    }

    for (const auto& managedDir : candidateManagedDirs) {
        DWORD dwAttrib = GetFileAttributesA(managedDir.c_str());
        if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
            m_caps.engineName = "Unity (Managed)";

            if (FileExists(managedDir + "Photon3Unity3D.dll"))  m_caps.hasPhoton3Unity = true;
            if (FileExists(managedDir + "PhotonRealtime.dll"))  m_caps.hasPhotonRealtime = true;
            if (FileExists(managedDir + "PhotonVoice.dll"))     m_caps.hasPhotonVoice = true;
            if (FileExists(managedDir + "Fusion.Runtime.dll"))  m_caps.hasPhotonFusion = true;
            if (FileExists(managedDir + "Mirror.dll"))          m_caps.hasMirror = true;
            if (FileExists(managedDir + "kcp2k.dll"))           m_caps.hasKcp = true;
            break;
        }
    }
}

// -----------------------------------------------------------------------------
// Phase 4: IL2CPP Metadata Scan (Lightweight Header & String Read)
// -----------------------------------------------------------------------------
#pragma pack(push, 4)
struct Il2CppGlobalMetadataHeader {
    uint32_t sanity;                  // 0xFAB11BAF
    int32_t  version;
    int32_t  stringLiteralOffset;
    int32_t  stringLiteralSize;
    int32_t  stringLiteralDataOffset;
    int32_t  stringLiteralDataSize;
    int32_t  stringOffset;
    int32_t  stringSize;
};
#pragma pack(pop)

void CapabilityDetector::ScanPhase4_IL2CPPMetadata() {
    std::string exeDir = GetExecutableDirectory();
    std::string exeBase = GetExecutableBaseName();

    std::vector<std::string> candidateMeta = {
        exeDir + exeBase + "_Data\\il2cpp_data\\Metadata\\global-metadata.dat",
        exeDir + "Data\\il2cpp_data\\Metadata\\global-metadata.dat"
    };

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA((exeDir + "*_Data").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            candidateMeta.push_back(exeDir + fd.cFileName + "\\il2cpp_data\\Metadata\\global-metadata.dat");
        }
        FindClose(hFind);
    }

    for (const auto& metaPath : candidateMeta) {
        if (!FileExists(metaPath)) continue;

        m_caps.isIL2CPP = true;
        m_caps.engineName = "Unity (IL2CPP)";

        FILE* fp = nullptr;
        if (fopen_s(&fp, metaPath.c_str(), "rb") != 0 || !fp) continue;

        Il2CppGlobalMetadataHeader hdr{};
        if (fread(&hdr, sizeof(hdr), 1, fp) == 1 && hdr.sanity == 0xFAB11BAF) {
            // Read string section (cap at 8MB for speed)
            if (hdr.stringOffset > 0 && hdr.stringSize > 0) {
                int readSize = hdr.stringSize;
                if (readSize > 8 * 1024 * 1024) readSize = 8 * 1024 * 1024;

                if (fseek(fp, hdr.stringOffset, SEEK_SET) == 0) {
                    std::vector<char> strBuf(readSize + 1, 0);
                    size_t bytesRead = fread(strBuf.data(), 1, readSize, fp);
                    if (bytesRead > 0) {
                        std::string metaContent(strBuf.data(), bytesRead);

                        if (metaContent.find("Photon.Pun") != std::string::npos ||
                            metaContent.find("ConnectToRegionMaster") != std::string::npos ||
                            metaContent.find("PhotonHandler") != std::string::npos) {
                            m_caps.il2cppHasPhotonPUN = true;
                        }
                        if (metaContent.find("Fusion.NetworkRunner") != std::string::npos ||
                            metaContent.find("Fusion.Runtime") != std::string::npos) {
                            m_caps.il2cppHasFusion = true;
                        }
                        if (metaContent.find("Mirror.NetworkManager") != std::string::npos) {
                            m_caps.il2cppHasMirror = true;
                        }
                        if (metaContent.find("Photon.Voice") != std::string::npos ||
                            metaContent.find("VoiceClient") != std::string::npos) {
                            m_caps.hasPhotonVoice = true;
                        }
                    }
                }
            }
        }
        fclose(fp);
        break;
    }
}

// -----------------------------------------------------------------------------
// Classifier & Main Scan Entry
// -----------------------------------------------------------------------------
NetworkTopology CapabilityDetector::ClassifyTopology() {
    // Topología B: Multiplexado Híbrido (Fusion + nanosockets, Mirror + EOS WebRTC)
    // Se prioriza sobre Topología C para evitar que juegos híbridos con soporte auxiliar de voz
    // (como Dumb Ways to Build) sean erróneamente forzados a Cloud Relay estricto.
    if (m_caps.hasPhotonFusion || m_caps.il2cppHasFusion || m_caps.hasNanosockets ||
        m_caps.hasMirror || m_caps.il2cppHasMirror || m_caps.hasEOS || m_caps.hasKcp) {
        m_caps.topology = NetworkTopology::TopologyB_HybridMultiplex;
    }
    // Topología C: PUN 2 / Photon Voice requiere servidor relay obligatorio
    else if (m_caps.hasPhotonRealtime || m_caps.hasPhoton3Unity || m_caps.il2cppHasPhotonPUN || m_caps.hasPhotonVoice) {
        m_caps.topology = NetworkTopology::TopologyC_CloudRelayStrict;
    }
    // Topología A: Sockets Directos (P2P Puro / LAN / Valve Steam SDR)
    else {
        m_caps.topology = NetworkTopology::TopologyA_DirectP2P;
    }

    return m_caps.topology;
}

const GameCapabilities& CapabilityDetector::ScanCapabilities() {
    if (m_scanned) return m_caps;

    ScanPhase1_IAT();
    ScanPhase2_LoadedModules();
    ScanPhase3_UnityManaged();
    ScanPhase4_IL2CPPMetadata();
    ClassifyTopology();

    m_scanned = true;

    ReFixLog("[UNAE:Detector] Runtime Capabilities Scan Complete:");
    ReFixLog("  -> Engine Detected: %s", m_caps.engineName.c_str());
    ReFixLog("  -> Topology Classified: %s", TopologyToString(m_caps.topology));
    ReFixLog("  -> Modules: ws2_32=%d, nanosockets=%d, steam_api64=%d, EOS=%d",
        m_caps.hasWinsock, m_caps.hasNanosockets, m_caps.hasSteamApi, m_caps.hasEOS);
    ReFixLog("  -> Assemblies: PhotonRealtime=%d, PhotonVoice=%d, Fusion=%d, Mirror=%d, KCP=%d",
        m_caps.hasPhotonRealtime, m_caps.hasPhotonVoice, m_caps.hasPhotonFusion, m_caps.hasMirror, m_caps.hasKcp);
    if (m_caps.isIL2CPP) {
        ReFixLog("  -> IL2CPP Scan: PUN=%d, Fusion=%d, Mirror=%d",
            m_caps.il2cppHasPhotonPUN, m_caps.il2cppHasFusion, m_caps.il2cppHasMirror);
    }

    return m_caps;
}

} // namespace UNAE
