// MMVR Finger Probe V0. See NOTICE.md for Toyxyz inspiration and Valve SDK attribution.
#include "OscSender.h"
#include <windows.h>
#include <psapi.h>
#include <openvr.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;
constexpr const char* SetPath = "/actions/mmvrfingerprobe";
constexpr std::array<const char*,2> ActionPaths{"/actions/mmvrfingerprobe/in/skeletonleft", "/actions/mmvrfingerprobe/in/skeletonright"};
constexpr std::array<const char*,2> SourcePaths{"/user/hand/left", "/user/hand/right"};
constexpr std::array<const char*,2> SideNames{"LEFT", "RIGHT"};
constexpr std::array<vr::ETrackedControllerRole,2> Roles{vr::TrackedControllerRole_LeftHand, vr::TrackedControllerRole_RightHand};
constexpr int NotCalled = -1;
HANDLE stopEvent = nullptr;
BOOL WINAPI StopHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        if (stopEvent) SetEvent(stopEvent);
        return TRUE;
    }
    return FALSE;
}
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit Handle(HANDLE h) : value(h) {}
};
struct Options {
    double hz = 90.0, duration = 0.0;
    bool debug = false, selfTest = false, timingTest = false, syntheticOsc = false, osc = false, oscSplays = false, help = false;
    int oscPort = 19761;
    vr::EVRSummaryType summary = vr::VRSummaryType_FromDevice;
};
double Number(const std::string& text) {
    size_t used = 0;
    const double n = std::stod(text, &used);
    if (used != text.size() || !std::isfinite(n)) throw std::runtime_error("Invalid numeric argument");
    return n;
}
Options Parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help") o.help = true;
        else if (a == "--debug") o.debug = true;
        else if (a == "--self-test") o.selfTest = true;
        else if (a == "--timing-test") o.timingTest = true;
        else if (a == "--osc") o.osc = true;
        else if (a == "--osc-splays") { o.oscSplays = true; o.osc = true; }
        else if (a == "--osc-test") { o.syntheticOsc = true; o.osc = true; }
        else if (a == "--hz" || a == "--duration" || a == "--summary" || a == "--osc-port") {
            if (++i == argc) throw std::runtime_error("Missing value for " + a);
            const std::string v = argv[i];
            if (a == "--osc-port") { const double n = Number(v); if (n < 1 || n > 65535 || std::floor(n) != n) throw std::runtime_error("Invalid OSC port"); o.oscPort = static_cast<int>(n); }
            else if (a == "--hz") o.hz = Number(v);
            else if (a == "--duration") o.duration = Number(v);
            else if (v == "device") o.summary = vr::VRSummaryType_FromDevice;
            else if (v == "animation") o.summary = vr::VRSummaryType_FromAnimation;
            else throw std::runtime_error("Summary must be device or animation");
        } else throw std::runtime_error("Unknown option: " + a);
    }
    if (o.hz < 1.0 || o.hz > 500.0) throw std::runtime_error("--hz must be 1..500");
    if (o.duration < 0.0 || o.duration > 86400.0) throw std::runtime_error("--duration must be 0..86400 (0 = until Ctrl+C)");
    if ((o.timingTest || o.syntheticOsc) && o.duration == 0.0) o.duration = 3.0;
    return o;
}
const char* ErrorName(int e) {
    switch (e) {
    case NotCalled: return "NOT_CALLED";
    case 0: return "None"; case 1: return "NameNotFound"; case 2: return "WrongType";
    case 3: return "InvalidHandle"; case 4: return "InvalidParam"; case 5: return "NoSteam";
    case 6: return "MaxCapacityReached"; case 7: return "IPCError"; case 8: return "NoActiveActionSet";
    case 9: return "InvalidDevice"; case 10: return "InvalidSkeleton"; case 11: return "InvalidBoneCount";
    case 12: return "InvalidCompressedData"; case 13: return "NoData"; case 14: return "BufferTooSmall";
    case 15: return "MismatchedActionManifest"; case 16: return "MissingSkeletonData";
    case 17: return "InvalidBoneIndex";
    default: return "UnknownEVRInputError";
    }
}
void Error(const char* api, int e) {
    std::cout << api << '=' << e << '(' << ErrorName(e) << ") ";
}
struct CurlCheck { bool finite = true, outsideRange = false; };
CurlCheck CheckCurls(const std::array<float, 5>& curls) {
    CurlCheck result;
    for (float v : curls) {
        result.finite = result.finite && std::isfinite(v);
        result.outsideRange = result.outsideRange || v < 0.0f || v > 1.0f;
    }
    return result;
}
bool CorrectOrigin(bool connected, vr::ETrackedDeviceClass cls, vr::ETrackedControllerRole role,
                   vr::VRInputValueHandle_t actual, vr::VRInputValueHandle_t expected,
                   const std::string& type, vr::ETrackedControllerRole expectedRole = vr::TrackedControllerRole_LeftHand) {
    return connected && cls == vr::TrackedDeviceClass_Controller && role == expectedRole
        && expected != vr::k_ulInvalidInputValueHandle && actual == expected && type == "knuckles";
}
int SelfTest() {
    std::array<float, 5> values{0.0f, 0.2f, 0.5f, 0.9f, 1.0f};
    auto require = [](bool ok) { if (!ok) throw std::runtime_error("Self-test failed"); };
    require(CheckCurls(values).finite && !CheckCurls(values).outsideRange);
    require(CheckCurls(values).finite); // unchanged values remain valid
    values[0] = std::numeric_limits<float>::quiet_NaN(); require(!CheckCurls(values).finite);
    values[0] = std::numeric_limits<float>::infinity(); require(!CheckCurls(values).finite);
    values[0] = -0.1f; require(CheckCurls(values).finite && CheckCurls(values).outsideRange && values[0] == -0.1f);
    values[0] = 1.2f; require(CheckCurls(values).finite && CheckCurls(values).outsideRange && values[0] == 1.2f);
    require(CorrectOrigin(true, vr::TrackedDeviceClass_Controller, vr::TrackedControllerRole_LeftHand, 5, 5, "knuckles"));
    require(!CorrectOrigin(true, vr::TrackedDeviceClass_HMD, vr::TrackedControllerRole_LeftHand, 5, 5, "knuckles"));
    require(!CorrectOrigin(false, vr::TrackedDeviceClass_Controller, vr::TrackedControllerRole_LeftHand, 5, 5, "knuckles"));
    require(!CorrectOrigin(true, vr::TrackedDeviceClass_Controller, vr::TrackedControllerRole_RightHand, 5, 5, "knuckles"));
    require(!CorrectOrigin(true, vr::TrackedDeviceClass_Controller, vr::TrackedControllerRole_LeftHand, 6, 5, "knuckles"));
    require(!CorrectOrigin(true, vr::TrackedDeviceClass_Controller, vr::TrackedControllerRole_LeftHand, 0, 0, "knuckles"));
    require(!CorrectOrigin(true, vr::TrackedDeviceClass_Controller, vr::TrackedControllerRole_LeftHand, 5, 5, "vive_controller"));
    require(CorrectOrigin(true, vr::TrackedDeviceClass_Controller, vr::TrackedControllerRole_RightHand, 7, 7, "knuckles", vr::TrackedControllerRole_RightHand));
    require(!CorrectOrigin(true, vr::TrackedDeviceClass_Controller, vr::TrackedControllerRole_LeftHand, 7, 7, "knuckles", vr::TrackedControllerRole_RightHand));
    require(!CorrectOrigin(true, vr::TrackedDeviceClass_Controller, vr::TrackedControllerRole_RightHand, 5, 7, "knuckles", vr::TrackedControllerRole_RightHand));
    std::cout << "Self-test PASS: finite/range validation, stationary values, disconnected/wrong/ambiguous origin.\n"
                 "Synthetic checks only; no OpenVR session or hardware data.\n";
    return 0;
}
std::filesystem::path ExeDirectory() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) throw std::runtime_error("Cannot resolve executable location");
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}
struct Sample {
    int update = NotCalled, action = NotCalled, originError = NotCalled, summary = NotCalled, levelError = NotCalled;
    int propertyError = NotCalled;
    vr::InputSkeletalActionData_t data{};
    vr::InputOriginInfo_t origin{};
    vr::EVRSkeletalTrackingLevel level = vr::VRSkeletalTracking_Estimated;
    vr::ETrackedControllerRole role = vr::TrackedControllerRole_Invalid;
    bool connected = false, valid = false, outsideRange = false;
    std::string type, reason = "no sample";
    std::array<float, 5> curls{};
    std::array<float, 4> splays{};
    bool splayValid = false, splayOutsideRange = false;
    Clock::time_point acquired{};
    int64_t acquisitionQpc = FingerQpc(); // timestamp of invalid acquisition attempt unless summary succeeds
    Sample() { origin.trackedDeviceIndex = vr::k_unTrackedDeviceIndexInvalid; }
};
Sample Acquire(vr::IVRSystem* system, vr::IVRInput* input, int updateError,
               vr::VRActionHandle_t action, vr::VRInputValueHandle_t expectedSource, vr::ETrackedControllerRole expectedRole, vr::EVRSummaryType summaryType) {
    Sample s; // no previous curls survive an invalid poll
    s.update = updateError; // one shared UpdateActionState per polling cycle
    if (s.update != 0) { s.reason = "UpdateActionState failed"; return s; }
    s.action = input->GetSkeletalActionData(action, &s.data, sizeof(s.data));
    if (s.action != 0) { s.reason = "GetSkeletalActionData failed"; return s; }
    if (!s.data.bActive) { s.reason = "skeletal action inactive"; return s; }
    if (s.data.activeOrigin == vr::k_ulInvalidInputValueHandle) { s.reason = "active origin absent"; return s; }
    s.originError = input->GetOriginTrackedDeviceInfo(s.data.activeOrigin, &s.origin, sizeof(s.origin));
    s.levelError = input->GetSkeletalTrackingLevel(action, &s.level);
    vr::VRSkeletalSummaryData_t summary{};
    s.summary = input->GetSkeletalSummaryData(action, summaryType, &summary);
    s.acquisitionQpc = FingerQpc(); // summary read completion, before processing/transport
    if (s.originError != 0 || s.levelError != 0 || s.summary != 0) {
        s.reason = "origin/tracking-level/summary API failure"; return s;
    }
    const auto device = s.origin.trackedDeviceIndex;
    if (device >= vr::k_unMaxTrackedDeviceCount) { s.reason = "origin device index invalid"; return s; }
    s.connected = system->IsTrackedDeviceConnected(device);
    s.role = system->GetControllerRoleForTrackedDeviceIndex(device);
    char type[256]{};
    vr::ETrackedPropertyError propertyError = vr::TrackedProp_Success;
    system->GetStringTrackedDeviceProperty(device, vr::Prop_ControllerType_String, type, sizeof(type), &propertyError);
    s.propertyError = static_cast<int>(propertyError);
    s.type = type;
    if (propertyError != vr::TrackedProp_Success || !CorrectOrigin(s.connected,
        system->GetTrackedDeviceClass(device), s.role, s.origin.devicePath, expectedSource, s.type, expectedRole)) {
        s.reason = "origin is disconnected, ambiguous or not the expected hand Knuckles"; return s;
    }
    for (size_t i = 0; i < s.curls.size(); ++i) s.curls[i] = summary.flFingerCurl[i];
    const auto check = CheckCurls(s.curls);
    if (!check.finite) { s.reason = "non-finite curl rejected"; return s; }
    s.outsideRange = check.outsideRange;
    // Same FromDevice summary/acquisition, separate validity. Splay failure never rejects curls.
    s.splayValid = true;
    for (size_t i=0; i<s.splays.size(); ++i) {
        s.splays[i] = summary.flFingerSplay[i];
        s.splayValid = s.splayValid && std::isfinite(s.splays[i]);
        s.splayOutsideRange = s.splayOutsideRange || s.splays[i] < 0.0f || s.splays[i] > 1.0f;
    }
    s.acquired = Clock::now();
    s.valid = true;
    s.reason = "valid";
    return s;
}
uint64_t ProcessCpu100ns() {
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) return 0;
    const auto value = [](FILETIME t) {
        return (static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    };
    return value(kernel) + value(user);
}
void Memory() {
    PROCESS_MEMORY_COUNTERS_EX m{}; m.cb = sizeof(m);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m), sizeof(m)))
        std::cout << " privateMiB=" << static_cast<double>(m.PrivateUsage) / 1048576.0
                  << " workingMiB=" << static_cast<double>(m.WorkingSetSize) / 1048576.0;
}
int Run(const Options& options, Clock::time_point programStart) {
    Handle stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!stop.value) throw std::runtime_error("Cannot create stop event");
    stopEvent = stop.value;
    if (!SetConsoleCtrlHandler(StopHandler, TRUE)) throw std::runtime_error("Cannot install Ctrl+C handler");
    struct HandlerCleanup { ~HandlerCleanup() { SetConsoleCtrlHandler(StopHandler, FALSE); stopEvent = nullptr; } } cleanup;
    // High-resolution per-process waitable timer; no global timer resolution/power changes.
    Handle timer(CreateWaitableTimerExW(nullptr, nullptr, 0x00000002, TIMER_ALL_ACCESS));
    if (!timer.value) timer.value = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!timer.value) throw std::runtime_error("Cannot create polling timer");
    vr::IVRSystem* system = nullptr;
    vr::IVRInput* input = nullptr;
    std::array<vr::VRActionHandle_t,2> actions{};
    std::array<vr::VRInputValueHandle_t,2> sources{};
    vr::VRActiveActionSet_t set{}; // default priority 0; no device restriction or overlay override
    struct OptionalSession { bool initialized = false; ~OptionalSession() { if (initialized) vr::VR_Shutdown(); } } session;
    if (!options.timingTest && !options.syntheticOsc) {
        const auto manifest = ExeDirectory() / "config" / "actions.json";
        if (!std::filesystem::is_regular_file(manifest) ||
            !std::filesystem::is_regular_file(manifest.parent_path() / "bindings_knuckles.json"))
            throw std::runtime_error("Probe manifest/binding missing beside executable");
        std::cout << "OpenVR SDK 1.5.17 | VRApplication_Background | LEFT + RIGHT | summary="
                  << (options.summary == vr::VRSummaryType_FromDevice ? "FromDevice" : "FromAnimation")
                  << " | targetHz=" << options.hz << "\nmanifest=" << manifest.u8string() << '\n';
        std::cout << "runtimeInstalled=" << vr::VR_IsRuntimeInstalled() << '\n';
        vr::EVRInitError initError = vr::VRInitError_None;
        system = vr::VR_Init(&initError, vr::VRApplication_Background);
        const double startupMs = std::chrono::duration<double, std::milli>(Clock::now() - programStart).count();
        std::cout << "VR_Init=" << static_cast<int>(initError) << '(' << vr::VR_GetVRInitErrorAsSymbol(initError)
                  << ") startupMs=" << startupMs;
        Memory(); std::cout << '\n';
        session.initialized = initError == vr::VRInitError_None;
        if (initError != vr::VRInitError_None || !system) {
            std::cerr << "OpenVR Background unavailable: " << vr::VR_GetVRInitErrorAsEnglishDescription(initError)
                      << "\nStart SteamVR manually, then retry. No runtime launch/restart attempted.\n";
            return 2;
        }
        input = vr::VRInput();
        if (!input) throw std::runtime_error("IVRInput_007 unavailable");
        // MUST precede PollNextEvent and UpdateActionState. Never reset/reload other apps' manifests.
        const auto manifestUtf8 = manifest.u8string();
        const int manifestError = input->SetActionManifestPath(manifestUtf8.c_str());
        Error("SetActionManifestPath", manifestError); std::cout << '\n';
        if (manifestError != 0) return 3;
        const int setError = input->GetActionSetHandle(SetPath, &set.ulActionSet);
        Error("GetActionSetHandle", setError); std::cout << '\n';
        if (setError || !set.ulActionSet) return 3;
        for (size_t side=0; side<2; ++side) {
            const int actionError = input->GetActionHandle(ActionPaths[side], &actions[side]);
            const int sourceError = input->GetInputSourceHandle(SourcePaths[side], &sources[side]);
            std::cout << SideNames[side] << ' '; Error("GetActionHandle",actionError); Error("GetInputSourceHandle",sourceError);
            std::cout << " action=0x" << std::hex << actions[side] << " source=0x" << sources[side] << std::dec << '\n';
            if (actionError || sourceError || !actions[side] || !sources[side]) return 3;
        }
        char appKey[vr::k_unMaxApplicationKeyLength]{};
        if (auto apps = vr::VRApplications()) {
            const auto appError = apps->GetApplicationKeyByProcessId(GetCurrentProcessId(), appKey, sizeof(appKey));
            std::cout << "Own application identity lookup=" << static_cast<int>(appError) << " key=" << appKey
                      << " (no app registration/identity override performed)\n";
        }
    } else std::cout << (options.syntheticOsc ? "SYNTHETIC OSC TEST:" : "SCHEDULER ONLY:") << " no OpenVR init, no hardware samples. targetHz=" << options.hz << '\n';

    OscSender osc(options.osc && !options.timingTest, options.oscPort);
    const auto period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / options.hz));
    const auto start = Clock::now();
    auto deadline = start, reportAt = start, windowStart = start;
    uint64_t polls = 0, windowPolls = 0, overruns = 0;
    std::array<uint64_t,2> validCount{}, invalidCount{}, invalidations{};
    uint64_t cpuLast = ProcessCpu100ns();
    std::array<bool,2> wasValid{};
    bool quit = false;
    uint32_t lastEvent = 0;
    while (WaitForSingleObject(stop.value, 0) == WAIT_TIMEOUT) {
        const auto now = Clock::now();
        if (options.duration > 0.0 && std::chrono::duration<double>(now - start).count() >= options.duration) break;
        osc.BeginCycle();
        std::array<Sample,2> samples{};
        if (!options.timingTest && !options.syntheticOsc) {
            const int updated = input->UpdateActionState(&set,sizeof(set),1);
            for (size_t side=0; side<2; ++side)
                samples[side] = Acquire(system,input,updated,actions[side],sources[side],Roles[side],options.summary);
            vr::VREvent_t event{};
            for (unsigned eventCount = 0; eventCount < 256 && system->PollNextEvent(&event, sizeof(event)); ++eventCount) {
                lastEvent = event.eventType;
                if (event.eventType == vr::VREvent_Quit) { quit = true; break; }
            }
        }
        for (size_t side=0; side<2 && !options.timingTest; ++side) {
            auto& sample=samples[side];
            if (options.syntheticOsc) {
                sample.valid=true; sample.levelError=0; sample.level=vr::VRSkeletalTracking_Partial;
                sample.curls = side == 0 ? std::array<float,5>{0.1f,0.2f,0.3f,0.4f,0.5f} : std::array<float,5>{0.6f,0.7f,0.8f,0.9f,1.0f};
                sample.splayValid = true;
                for (size_t i=0; i<sample.splays.size(); ++i) sample.splays[i] = static_cast<float>(i+1+side*4)*0.11f;
                sample.acquired=Clock::now(); sample.acquisitionQpc=FingerQpc();
            }
            if (sample.valid) ++validCount[side]; else ++invalidCount[side];
            if (wasValid[side] && !sample.valid) ++invalidations[side];
            wasValid[side]=sample.valid;
            osc.Send(side,sample.valid,sample.levelError == 0 ? static_cast<int>(sample.level) : -1,sample.curls,sample.acquisitionQpc);
            if (options.oscSplays) osc.SendSplays(side,sample.splayValid,sample.levelError == 0 ? static_cast<int>(sample.level) : -1,sample.splays,sample.acquisitionQpc);
        }
        ++polls; ++windowPolls;
        const auto after = Clock::now();
        if (after >= reportAt || quit) {
            const double windowSec = std::chrono::duration<double>(after - windowStart).count();
            const uint64_t cpu = ProcessCpu100ns();
            const double rate = windowSec >= 0.1 ? static_cast<double>(windowPolls) / windowSec : 0.0;
            const double cpuPercent = windowSec >= 0.1 ? static_cast<double>(cpu - cpuLast) / 100000.0 / windowSec : 0.0;
            std::cout << std::fixed << std::setprecision(2);
            if (options.timingTest) std::cout << "SCHEDULER ONLY";
            else for (size_t side=0; side<2; ++side) {
                const auto& sample=samples[side];
                if (side) std::cout << "\n";
                if (options.syntheticOsc) std::cout << "SYNTHETIC / NO HARDWARE: ";
                const double ageMs = sample.valid ? std::chrono::duration<double, std::milli>(after - sample.acquired).count() : -1.0;
                const bool fresh = sample.valid && ageMs <= (2000.0 / options.hz + 50.0);
                if (fresh) {
                    std::cout << SideNames[side] << " VALID T=" << sample.curls[vr::VRFinger_Thumb] << " I=" << sample.curls[vr::VRFinger_Index]
                        << " M=" << sample.curls[vr::VRFinger_Middle] << " R=" << sample.curls[vr::VRFinger_Ring]
                        << " P=" << sample.curls[vr::VRFinger_Pinky] << " ageMs=" << ageMs;
                    if (sample.outsideRange) std::cout << " RANGE_WARNING(raw values preserved)";
                } else std::cout << SideNames[side] << " INVALID: " << (sample.valid ? "local acquisition stale" : sample.reason);
                // DEBUG reports every call; NORMAL reports errors whenever acquisition is invalid.
                if (options.debug || !sample.valid) {
                    std::cout << " | ";
                    Error("UpdateActionState", sample.update); Error("GetSkeletalActionData", sample.action);
                    Error("GetOriginTrackedDeviceInfo", sample.originError); Error("GetSkeletalTrackingLevel", sample.levelError);
                    Error("GetSkeletalSummaryData", sample.summary);
                    std::cout << "active=" << sample.data.bActive << " origin=0x" << std::hex << sample.data.activeOrigin << std::dec << " propertyError=" << sample.propertyError;
                }
                if (options.debug && !options.syntheticOsc) {
                    std::cout << "\n  device=" << sample.origin.trackedDeviceIndex << " connected=" << sample.connected
                        << " role=" << static_cast<int>(sample.role) << " type=" << sample.type
                        << " propertyError=" << sample.propertyError << " devicePath=0x" << std::hex << sample.origin.devicePath << std::dec
                        << " trackingLevel=" << (sample.levelError == 0 ? static_cast<int>(sample.level) : -1) << " (0 Estimated / 1 Partial / 2 Full; capability, not confidence)"
                        << " available=" << system->IsInputAvailable()
                        << " dashboard=" << (vr::VROverlay() ? static_cast<int>(vr::VROverlay()->IsDashboardVisible()) : -1)
                        << " hmdActivity=" << static_cast<int>(system->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd))
                        << " lastEvent=" << lastEvent;
                }
            }
            std::cout << " | effectiveHz=" << rate << " polls=" << polls << " Lvalid=" << validCount[0] << " Rvalid=" << validCount[1]
                << " Linvalid=" << invalidCount[0] << " Rinvalid=" << invalidCount[1] << " Linvalidations=" << invalidations[0] << " Rinvalidations=" << invalidations[1] << " overruns=" << overruns
                << " cpuPercentOneCore=" << cpuPercent;
            osc.Diagnostics(); Memory(); std::cout << '\n' << std::flush;
            cpuLast = cpu; windowStart = after; windowPolls = 0; reportAt = after + std::chrono::seconds(1);
        }
        if (quit) { std::cout << "OpenVR Quit event: closing this client only.\n"; break; }
        deadline += period;
        if (deadline <= Clock::now()) { ++overruns; deadline = Clock::now() + period; }
        const auto wait100ns = std::chrono::duration_cast<std::chrono::duration<long long, std::ratio<1, 10000000>>>(deadline - Clock::now()).count();
        LARGE_INTEGER due{}; due.QuadPart = -((wait100ns > 0) ? wait100ns : 1);
        if (!SetWaitableTimer(timer.value, &due, 0, nullptr, nullptr, FALSE)) throw std::runtime_error("Polling timer failed");
        HANDLE waits[]{stop.value, timer.value};
        const DWORD waited = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (waited == WAIT_OBJECT_0) break;
        if (waited != WAIT_OBJECT_0 + 1) throw std::runtime_error("Polling wait failed");
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    std::cout << "Stopped cleanly. elapsedSec=" << elapsed << " polls=" << polls
        << " meanHz=" << (elapsed > 0.0 ? static_cast<double>(polls) / elapsed : 0.0)
        << " Lvalid=" << validCount[0] << " Rvalid=" << validCount[1] << " Linvalid=" << invalidCount[0] << " Rinvalid=" << invalidCount[1] << " Linvalidations=" << invalidations[0] << " Rinvalidations=" << invalidations[1] << '\n';
    return 0;
}
int main(int argc, char** argv) {
    const auto start = Clock::now();
    try {
        const auto options = Parse(argc, argv);
        if (options.help) {
            std::cout << "MMVR_FingerProbe [--debug] [--hz 90] [--duration seconds] [--summary device|animation]\n"
                         "Default: LEFT + RIGHT Knuckles, FromDevice, 90 Hz target, console about 1 Hz. Ctrl+C stops.\n"
                         "--self-test: synthetic validation only, no OpenVR.\n"
                         "--timing-test [--duration 3]: scheduler/idle metrics only, no OpenVR.\n"
                         "--osc [--osc-port 19761]: enable OSC V1 to 127.0.0.1 only.\n"
                         "--osc-splays: additive RAW TI/IM/MR/RP messages; existing curl payload unchanged.\n"
                         "--osc-test: SYNTHETIC distinct LEFT/RIGHT fixed curls, no OpenVR, bounded to 3s unless --duration provided.\n"
                         "No SteamVR auto-start, Unreal capture changes or animation.\n";
            return 0;
        }
        if (options.selfTest) return SelfTest();
        return Run(options, start);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n'; return 1;
    }
}
